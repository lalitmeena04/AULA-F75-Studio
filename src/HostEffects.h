#pragma once

#include <QColor>
#include <QList>
#include <QObject>
#include <QThread>
#include <QTimer>
#include <QVariantMap>
#include <atomic>
#include <memory>

// Host-rendered per-key lighting sources. Audio: WASAPI loopback of the default
// render device, split into frequency bands across key columns. Heatmap: CPU
// load (GetSystemTimes), GPU 3D-engine utilisation (PDH) and GPU temperature
// (NVML when nvidia's nvml.dll is present). Frames are 128 colours by LED index.
class HostEffects final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY changed)         // off, audio, heatmap
    Q_PROPERTY(double gain READ gain WRITE setGain NOTIFY changed)
    Q_PROPERTY(QColor baseColor READ baseColor WRITE setBaseColor NOTIFY changed)
    Q_PROPERTY(bool rainbow READ rainbow WRITE setRainbow NOTIFY changed)
    Q_PROPERTY(int fps READ fps WRITE setFps NOTIFY changed)
    Q_PROPERTY(QVariantMap telemetry READ telemetry NOTIFY telemetryChanged)
    Q_PROPERTY(QString status READ status NOTIFY changed)
public:
    explicit HostEffects(QObject *parent = nullptr);
    ~HostEffects() override;

    QString mode() const { return m_mode; }
    void setMode(const QString &mode);
    double gain() const { return m_gain; }
    void setGain(double gain);
    QColor baseColor() const { return m_baseColor; }
    void setBaseColor(const QColor &color);
    bool rainbow() const { return m_rainbow; }
    void setRainbow(bool on);
    int fps() const { return m_fps; }
    void setFps(int fps);
    QVariantMap telemetry() const;
    QString status() const { return m_status; }

    // Current preview frame for the UI (same data streamed to hardware).
    Q_INVOKABLE QVariantList previewFrame() const;

signals:
    void changed();
    void telemetryChanged();
    void frameReady(QList<QColor> byLedIndex);

private:
    void tick();
    void startAudio();
    void stopAudio();
    void sampleSystem();
    QList<QColor> renderAudio() const;
    QList<QColor> renderHeatmap() const;
    QColor tone(double intensity, double hueShift) const;

    struct Audio;
    std::unique_ptr<Audio> m_audio;
    QTimer m_timer;
    QString m_mode = QStringLiteral("off");
    QString m_status;
    double m_gain = 1.0;
    QColor m_baseColor = QColor(0x39, 0xb6, 0xff);
    bool m_rainbow = true;
    int m_fps = 10;
    double m_cpu = 0, m_gpu = 0, m_gpuTemp = -1;
    double m_smoothCpu = 0, m_smoothGpu = 0;
    QList<QColor> m_lastFrame;
    double m_bands[16] = {};
    double m_peak = 0.02;
    quint64 m_prevIdle = 0, m_prevKernel = 0, m_prevUser = 0;
    void *m_pdhQuery = nullptr;
    void *m_pdhCounter = nullptr;
    int m_sampleDivider = 0;
    double m_phase = 0;
};
