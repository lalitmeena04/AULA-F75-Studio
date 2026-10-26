#include "HostEffectsController.h"
#include "KeyLayout.h"
#include "Protocol.h"

#include <QVariantMap>
#include <QtMath>
#include <qt_windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <pdh.h>
#include <atomic>
#include <cmath>
#include <mutex>
#include <thread>
#include <vector>

namespace {
constexpr int bandCount = 16;
// Logarithmic band centres from ~60 Hz to ~12 kHz.
double bandFrequency(int band) {
    return 60.0 * std::pow(200.0, band / double(bandCount - 1));
}

typedef int (*NvmlInit)();
typedef int (*NvmlShutdown)();
typedef int (*NvmlGetHandle)(unsigned, void**);
typedef int (*NvmlGetTemp)(void*, int, unsigned*);

struct Nvml {
        HMODULE module = nullptr;
        void* device = nullptr;
        NvmlGetTemp getTemp = nullptr;
        NvmlShutdown shutdown = nullptr;
        Nvml() {
            module = LoadLibraryW(L"nvml.dll");
            if(!module)
                module = LoadLibraryW(L"C:\\Program Files\\NVIDIA Corporation\\NVSMI\\nvml.dll");
            if(!module)
                return;
            auto init = reinterpret_cast<NvmlInit>(GetProcAddress(module, "nvmlInit_v2"));
            auto handle = reinterpret_cast<NvmlGetHandle>(GetProcAddress(module, "nvmlDeviceGetHandleByIndex_v2"));
            getTemp = reinterpret_cast<NvmlGetTemp>(GetProcAddress(module, "nvmlDeviceGetTemperature"));
            shutdown = reinterpret_cast<NvmlShutdown>(GetProcAddress(module, "nvmlShutdown"));
            if(!init || !handle || !getTemp || init() != 0 || handle(0, &device) != 0)
                device = nullptr;
        }
        ~Nvml() {
            if(module && shutdown && device)
                shutdown();
            if(module)
                FreeLibrary(module);
        }
        double temperature() const {
            unsigned t = 0;
            return device && getTemp(device, 0, &t) == 0 ? double(t) : -1.0;
        }
};
} // namespace

// WASAPI loopback capture on its own thread; publishes band magnitudes.
struct HostEffectsController::Audio {
        std::thread thread;
        std::atomic<bool> stop{false};
        std::mutex mutex;
        double bands[bandCount] = {};
        QString error;

        void run() {
            CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            IMMDeviceEnumerator* enumerator = nullptr;
            IMMDevice* device = nullptr;
            IAudioClient* client = nullptr;
            IAudioCaptureClient* capture = nullptr;
            WAVEFORMATEX* format = nullptr;
            auto fail = [&](const char* what) {
                std::lock_guard<std::mutex> lock(mutex);
                error = QStringLiteral("Audio capture unavailable (%1).").arg(QLatin1String(what));
            };
            if(FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                                       reinterpret_cast<void**>(&enumerator))))
                fail("MMDeviceEnumerator");
            else if(FAILED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device)))
                fail("no default output device");
            else if(FAILED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                            reinterpret_cast<void**>(&client))))
                fail("IAudioClient");
            else if(FAILED(client->GetMixFormat(&format)))
                fail("GetMixFormat");
            else if(FAILED(client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, 20 * 10000, 0,
                                              format, nullptr)))
                fail("Initialize loopback");
            else if(FAILED(client->GetService(__uuidof(IAudioCaptureClient), reinterpret_cast<void**>(&capture))))
                fail("IAudioCaptureClient");
            else if(FAILED(client->Start()))
                fail("Start");
            else {
                const bool isFloat =
                    format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT ||
                    (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
                     reinterpret_cast<WAVEFORMATEXTENSIBLE*>(format)->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);
                const int channels = format->nChannels;
                const double rate = format->nSamplesPerSec;
                std::vector<float> mono;
                mono.reserve(8192);
                while(!stop.load()) {
                    UINT32 packet = 0;
                    if(FAILED(capture->GetNextPacketSize(&packet)))
                        break;
                    if(packet == 0) {
                        Sleep(5);
                        continue;
                    }
                    BYTE* data = nullptr;
                    UINT32 frames = 0;
                    DWORD flags = 0;
                    if(FAILED(capture->GetBuffer(&data, &frames, &flags, nullptr, nullptr)))
                        break;
                    for(UINT32 i = 0; i < frames; ++i) {
                        float sample = 0;
                        if(flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                            sample = 0;
                        } else if(isFloat) {
                            const float* f = reinterpret_cast<const float*>(data) + i * channels;
                            for(int c = 0; c < channels; ++c)
                                sample += f[c];
                            sample /= channels;
                        } else if(format->wBitsPerSample == 16) {
                            const int16_t* s = reinterpret_cast<const int16_t*>(data) + i * channels;
                            for(int c = 0; c < channels; ++c)
                                sample += s[c] / 32768.0f;
                            sample /= channels;
                        }
                        mono.push_back(sample);
                    }
                    capture->ReleaseBuffer(frames);
                    if(mono.size() >= 2048) {
                        // Goertzel per band over the last 2048 samples with a Hann window.
                        const size_t n = 2048;
                        const size_t start = mono.size() - n;
                        double out[bandCount];
                        for(int b = 0; b < bandCount; ++b) {
                            const double w = 2.0 * M_PI * bandFrequency(b) / rate;
                            const double coeff = 2.0 * std::cos(w);
                            double s0 = 0, s1 = 0, s2 = 0;
                            for(size_t i = 0; i < n; ++i) {
                                const double window = 0.5 - 0.5 * std::cos(2.0 * M_PI * i / (n - 1));
                                s0 = mono[start + i] * window + coeff * s1 - s2;
                                s2 = s1;
                                s1 = s0;
                            }
                            const double power = s1 * s1 + s2 * s2 - coeff * s1 * s2;
                            out[b] = std::sqrt(std::max(0.0, power)) / n * (1.0 + b * 0.35);
                        }
                        std::lock_guard<std::mutex> lock(mutex);
                        for(int b = 0; b < bandCount; ++b)
                            bands[b] = out[b];
                        mono.erase(mono.begin(), mono.begin() + static_cast<long long>(mono.size() - 512));
                    }
                }
                client->Stop();
            }
            if(format)
                CoTaskMemFree(format);
            if(capture)
                capture->Release();
            if(client)
                client->Release();
            if(device)
                device->Release();
            if(enumerator)
                enumerator->Release();
            CoUninitialize();
        }
};

HostEffectsController::HostEffectsController(QObject* parent) : QObject(parent) {
    connect(&m_timer, &QTimer::timeout, this, &HostEffectsController::tick);
    m_lastFrame = QList<QColor>(F75::matrixLength, QColor());
}

HostEffectsController::~HostEffectsController() {
    stopAudio();
    if(m_pdhQuery)
        PdhCloseQuery(static_cast<PDH_HQUERY>(m_pdhQuery));
}

void HostEffectsController::setMode(const QString& mode) {
    if(mode == m_mode)
        return;
    m_mode = mode;
    m_status.clear();
    if(m_mode == QStringLiteral("audio"))
        startAudio();
    else
        stopAudio();
    if(m_mode == QStringLiteral("off"))
        m_timer.stop();
    else
        m_timer.start(1000 / std::clamp(m_fps, 2, 20));
    emit changed();
}

void HostEffectsController::setGain(double gain) {
    m_gain = std::clamp(gain, 0.1, 8.0);
    emit changed();
}
void HostEffectsController::setBaseColor(const QColor& color) {
    m_baseColor = color;
    emit changed();
}
void HostEffectsController::setRainbow(bool on) {
    m_rainbow = on;
    emit changed();
}
void HostEffectsController::setFps(int fps) {
    m_fps = std::clamp(fps, 2, 30);
    if(m_timer.isActive())
        m_timer.start(1000 / m_fps);
    emit changed();
}

QVariantMap HostEffectsController::telemetry() const {
    return {{QStringLiteral("cpu"), m_smoothCpu},
            {QStringLiteral("gpu"), m_smoothGpu},
            {QStringLiteral("gpuTemp"), m_gpuTemp},
            {QStringLiteral("peak"), m_peak}};
}

QVariantList HostEffectsController::previewFrame() const {
    QVariantList list;
    for(const QColor& c : m_lastFrame)
        list.append(c.isValid() ? c.name() : QStringLiteral("#000000"));
    return list;
}

void HostEffectsController::startAudio() {
    stopAudio();
    m_audio = std::make_unique<Audio>();
    m_audio->thread = std::thread([a = m_audio.get()] { a->run(); });
}

void HostEffectsController::stopAudio() {
    if(!m_audio)
        return;
    m_audio->stop = true;
    if(m_audio->thread.joinable())
        m_audio->thread.join();
    m_audio.reset();
}

void HostEffectsController::sampleSystem() {
    FILETIME idle, kernel, user;
    if(GetSystemTimes(&idle, &kernel, &user)) {
        auto toU64 = [](const FILETIME& t) { return (quint64(t.dwHighDateTime) << 32) | t.dwLowDateTime; };
        const quint64 i = toU64(idle), k = toU64(kernel), u = toU64(user);
        const quint64 total = (k - m_prevKernel) + (u - m_prevUser);
        if(m_prevKernel && total > 0)
            m_cpu = 100.0 * (1.0 - double(i - m_prevIdle) / double(total));
        m_prevIdle = i;
        m_prevKernel = k;
        m_prevUser = u;
    }
    if(!m_pdhQuery) {
        PDH_HQUERY query = nullptr;
        if(PdhOpenQueryW(nullptr, 0, &query) == ERROR_SUCCESS) {
            PDH_HCOUNTER counter = nullptr;
            if(PdhAddEnglishCounterW(query, L"\\GPU Engine(*engtype_3D)\\Utilization Percentage", 0, &counter) ==
               ERROR_SUCCESS) {
                m_pdhQuery = query;
                m_pdhCounter = counter;
                PdhCollectQueryData(query);
            } else {
                PdhCloseQuery(query);
            }
        }
    }
    if(m_pdhQuery && PdhCollectQueryData(static_cast<PDH_HQUERY>(m_pdhQuery)) == ERROR_SUCCESS) {
        DWORD bytes = 0, count = 0;
        PdhGetFormattedCounterArrayW(static_cast<PDH_HCOUNTER>(m_pdhCounter), PDH_FMT_DOUBLE, &bytes, &count, nullptr);
        std::vector<BYTE> buffer(bytes + 1);
        auto* items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(buffer.data());
        if(bytes && PdhGetFormattedCounterArrayW(static_cast<PDH_HCOUNTER>(m_pdhCounter), PDH_FMT_DOUBLE, &bytes,
                                                 &count, items) == ERROR_SUCCESS) {
            double sum = 0;
            for(DWORD i = 0; i < count; ++i)
                sum += items[i].FmtValue.doubleValue;
            m_gpu = std::clamp(sum, 0.0, 100.0);
        }
    }
    static Nvml nvml;
    m_gpuTemp = nvml.temperature();
    m_smoothCpu = m_smoothCpu * 0.7 + m_cpu * 0.3;
    m_smoothGpu = m_smoothGpu * 0.7 + m_gpu * 0.3;
    emit telemetryChanged();
}

QColor HostEffectsController::tone(double intensity, double hueShift) const {
    intensity = std::clamp(intensity, 0.0, 1.0);
    if(!m_rainbow) {
        QColor c = m_baseColor;
        return QColor::fromRgbF(float(c.redF() * intensity), float(c.greenF() * intensity),
                                float(c.blueF() * intensity));
    }
    const double hue = std::fmod(hueShift + m_phase, 1.0);
    return QColor::fromHsvF(hue, 1.0, intensity);
}

QList<QColor> HostEffectsController::renderAudio() const {
    QList<QColor> frame(F75::matrixLength, QColor(0, 0, 0));
    for(const F75::KeyInfo& key : F75::keyLayout()) {
        const double x = (key.rect.center().x() - 30.0) / (F75::layoutWidth - 60.0);
        const int band = std::clamp(int(x * bandCount), 0, bandCount - 1);
        // Rows: bottom row (y ~ 248) lights first, top row (y ~ 43) last.
        const double rowLevel = 1.0 - std::clamp((key.rect.center().y() - 30.0) / 230.0, 0.0, 1.0);
        const double level = std::clamp(m_bands[band] / m_peak * m_gain, 0.0, 1.0);
        const double lit = level >= rowLevel ? 1.0 : (level >= rowLevel - 0.2 ? (level - (rowLevel - 0.2)) / 0.2 : 0.0);
        frame[key.ledIndex] = tone(lit * 0.9 + 0.05, x);
    }
    return frame;
}

QList<QColor> HostEffectsController::renderHeatmap() const {
    QList<QColor> frame(F75::matrixLength, QColor(0, 0, 0));
    const double cpu = m_smoothCpu / 100.0;
    const double gpu = m_smoothGpu / 100.0;
    const double temp = m_gpuTemp > 0 ? std::clamp((m_gpuTemp - 30.0) / 60.0, 0.0, 1.0) : gpu;
    for(const F75::KeyInfo& key : F75::keyLayout()) {
        const double x = (key.rect.center().x() - 30.0) / (F75::layoutWidth - 60.0);
        const double y = std::clamp((key.rect.center().y() - 30.0) / 230.0, 0.0, 1.0);
        // Left half: CPU fill from left; right half: GPU fill from right. Hue from cool to hot by temperature.
        const bool leftHalf = x < 0.5;
        const double fill = leftHalf ? cpu : gpu;
        const double pos = leftHalf ? x / 0.5 : (1.0 - x) / 0.5;
        const double lit = pos <= fill ? 1.0 : 0.08;
        const double hue = 0.62 - 0.62 * (leftHalf ? std::max(temp * 0.4, cpu) : temp); // blue -> red
        frame[key.ledIndex] = QColor::fromHsvF(float(std::clamp(hue, 0.0, 0.66)), 0.95f,
                                               float(std::clamp(lit * (0.35 + 0.65 * (1.0 - y * 0.3)), 0.0, 1.0)));
    }
    return frame;
}

void HostEffectsController::tick() {
    m_phase = std::fmod(m_phase + 0.004, 1.0);
    if(m_mode == QStringLiteral("audio") && m_audio) {
        {
            std::lock_guard<std::mutex> lock(m_audio->mutex);
            if(!m_audio->error.isEmpty() && m_status != m_audio->error) {
                m_status = m_audio->error;
                emit changed();
            }
            for(int b = 0; b < bandCount; ++b)
                m_bands[b] = m_bands[b] * 0.4 + m_audio->bands[b] * 0.6;
        }
        double maxBand = 0;
        for(double b : m_bands)
            maxBand = std::max(maxBand, b);
        m_peak = std::max(0.01, std::max(maxBand, m_peak * 0.995));
        m_lastFrame = renderAudio();
    } else if(m_mode == QStringLiteral("heatmap")) {
        if(++m_sampleDivider % std::max(1, m_fps / 2) == 0)
            sampleSystem();
        m_lastFrame = renderHeatmap();
    } else {
        return;
    }
    emit frameReady(m_lastFrame);
}
