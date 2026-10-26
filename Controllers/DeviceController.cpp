#include "DeviceController.h"
#include "HidTransport.h"
#include "HostEffectsController.h"
#include "KeyLayout.h"
#include "Logging.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QThread>
#include <QtConcurrent>
#include <QtEndian>

using namespace F75;

namespace {
constexpr int fnLayer = 1;

struct Session {
        std::unique_ptr<HidTransport> transport;
        QStringList log;
        QString error;
        QString link = QStringLiteral("none");

        bool open() {
            QString err;
            transport =
                HidTransport::open({usbVendorId, usbProductId, commandUsagePage, commandUsage, reportLength}, err);
            if(transport) {
                link = QStringLiteral("usb");
                log << QStringLiteral("Opened USB command interface (%1-byte feature reports)").arg(reportLength);
                return true;
            }
            const bool receiver = !HidTransport::enumerate(receiverVendorId, receiverProductId, err).isEmpty();
            const bool keyboard = !HidTransport::enumerate(usbVendorId, usbProductId, err).isEmpty();
            if(keyboard)
                error = QStringLiteral(
                    "The F75 is on USB but exposes no FF00/0001 interface with 520-byte feature reports.");
            else if(receiver) {
                link = QStringLiteral("receiver");
                error = QStringLiteral(
                    "Only the 2.4 GHz receiver is present. It carries typing and a 20-byte status channel, "
                    "but the configuration interface exists only over the USB cable. Plug the keyboard in to change "
                    "settings.");
            } else
                error = QStringLiteral("No AULA F75 keyboard or receiver is connected.");
            if(!err.isEmpty())
                error += QStringLiteral(" (%1)").arg(err);
            return false;
        }

        std::optional<QByteArray> read(Command command, uint8_t sub, int length) {
            QByteArray out;
            const int pages = (length + payloadLength - 1) / payloadLength;
            for(int page = 0; page < pages; ++page) {
                const QByteArray request = readRequest(command, sub, length, page);
                log << QStringLiteral("→ %1").arg(hex(request, 8));
                QThread::msleep(20);
                if(!transport->setFeature(request, error))
                    return std::nullopt;
                QThread::msleep(20);
                auto report = transport->getFeature(reportId, reportLength, error);
                if(!report)
                    return std::nullopt;
                auto frame = decodeFrame(*report);
                if(!frame) {
                    error = QStringLiteral("Response had an unexpected report ID.");
                    return std::nullopt;
                }
                log << QStringLiteral("← %1 | %2").arg(hex(report->left(8)), hex(report->mid(8), 16));
                out += report->mid(8, std::min(payloadLength, length - page * payloadLength));
            }
            return out;
        }

        bool write(Command command, uint8_t sub, const QByteArray& payload) {
            for(const QByteArray& report : encodePages(command, sub, payload)) {
                log << QStringLiteral("→ %1 | %2 (%3 bytes)")
                           .arg(hex(report.left(8)), hex(report.mid(8), 16))
                           .arg(payload.size());
                QThread::msleep(20);
                if(!transport->setFeature(report, error))
                    return false;
            }
            QThread::msleep(50);
            return true;
        }
};

struct Verified {
        Config config;
        Variant variant;
};

QVariantMap describe(const Config& config, const Variant& variant) {
    const Effect* effect = effectByHardwareId(config.effectHardwareId);
    const int effectIndex = effect ? effect->index : -1;
    const SlotSettings slot = effectIndex >= 0 ? config.slot[effectIndex] : SlotSettings{};
    int sleepSeconds = -1;
    for(const SleepPreset& p : sleepPresets())
        if(p.hardware == config.sleepHardware)
            sleepSeconds = p.seconds;
    return {
        {QStringLiteral("variant"), variant.name},
        {QStringLiteral("wireless"), variant.wireless},
        {QStringLiteral("effectIndex"), effectIndex},
        {QStringLiteral("effectName"), effect ? QLatin1String(effect->name) : QStringLiteral("Unknown")},
        {QStringLiteral("effectHardwareId"), config.effectHardwareId},
        {QStringLiteral("customMode"), config.customMode},
        {QStringLiteral("brightness"), slot.brightness},
        {QStringLiteral("speed"), slot.speed},
        {QStringLiteral("color"), slot.color},
        {QStringLiteral("sleepHardware"), config.sleepHardware},
        {QStringLiteral("sleepSeconds"), sleepSeconds},
        {QStringLiteral("configHex"), hex(config.raw)},
        {QStringLiteral("readAt"), QDateTime::currentDateTime().toString(Qt::ISODate)},
    };
}

std::optional<Verified> verify(Session& session, DeviceController::Outcome& outcome) {
    if(!session.open()) {
        outcome.state =
            session.link == QStringLiteral("receiver") ? QStringLiteral("receiver") : QStringLiteral("absent");
        outcome.link = session.link;
        outcome.message = session.error;
        return std::nullopt;
    }
    outcome.link = session.link;
    const auto identity = session.read(Command::Identity, 0x01, identityLength);
    if(!identity) {
        outcome.state = QStringLiteral("error");
        outcome.message = QStringLiteral("Identity read failed: %1").arg(session.error);
        return std::nullopt;
    }
    const auto variant = identifyVariant(*identity);
    if(!variant) {
        outcome.state = QStringLiteral("mismatch");
        outcome.message =
            QStringLiteral("Identity %1 does not match a known F75 variant. Writes stay disabled.").arg(hex(*identity));
        return std::nullopt;
    }
    const auto raw = session.read(Command::Config, 0, configLength);
    if(!raw) {
        outcome.state = QStringLiteral("error");
        outcome.message = QStringLiteral("Config read failed: %1").arg(session.error);
        return std::nullopt;
    }
    const auto config = decodeConfig(*raw);
    if(!config) {
        outcome.state = QStringLiteral("mismatch");
        outcome.message = QStringLiteral("Config block lacks the 5A A5 signature (%1). Writes stay disabled.")
                              .arg(hex(raw->left(16)));
        return std::nullopt;
    }
    return Verified{*config, *variant};
}

// Writes config, reads it back, fills outcome. Returns true when the readback decoded.
bool writeConfig(Session& session, DeviceController::Outcome& outcome, const Verified& verified, const QByteArray& next,
                 const QString& what) {
    outcome.wrote = true;
    if(!session.write(Command::Config, 0, next)) {
        outcome.state = QStringLiteral("error");
        outcome.message = QStringLiteral("%1: config write failed: %2").arg(what, session.error);
        return false;
    }
    const auto readback = session.read(Command::Config, 0, configLength);
    const auto decoded = readback ? decodeConfig(*readback) : std::nullopt;
    if(!decoded) {
        outcome.state = QStringLiteral("error");
        outcome.message = QStringLiteral("%1 sent but readback failed: %2").arg(what, session.error);
        return false;
    }
    outcome.state = QStringLiteral("verified");
    outcome.message =
        *readback == next
            ? QStringLiteral("%1 applied and confirmed by readback.").arg(what)
            : QStringLiteral("%1 accepted; device reports a different config, showing what it holds.").arg(what);
    outcome.current = describe(*decoded, verified.variant);
    return true;
}

uint32_t bswap(uint32_t v) {
    return qbswap(v);
}

// Factory Fn-layer defaults, keyed by key number, as the hardware word.
uint32_t fnDefault(const KeyInfo& key) {
    if(key.isFn)
        return 0x0000000d;
    switch(key.id) {
    case 1:
        return bswap(0x07000004);
    case 2:
        return bswap(0x02000223);
    case 3:
        return bswap(0x0200018a);
    case 4:
        return bswap(0x0004002B);
    case 5:
        return bswap(0x00080008);
    case 6:
        return bswap(0x08030200);
    case 7:
        return bswap(0x08030100);
    case 8:
        return bswap(0x020000b6);
    case 9:
        return bswap(0x020000cd);
    case 10:
        return bswap(0x020000b5);
    case 11:
        return bswap(0x020000E2);
    case 12:
        return bswap(0x020000ea);
    case 13:
        return bswap(0x020000e9);
    case 15:
        return bswap(0x07000008);
    case 16:
        return bswap(0x07000005);
    case 17:
        return bswap(0x07000006);
    case 18:
        return bswap(0x07000007);
    case 30:
        return bswap(0x08020000);
    case 31:
        return bswap(0x07000019);
    case 32:
        return bswap(0x07000018);
    case 33:
        return bswap(0x0700001A);
    case 43:
        return bswap(0x08000000);
    case 50:
        return bswap(0x0700000a);
    case 63:
        return bswap(0x07000011);
    case 72:
        return bswap(0x07000001);
    case 70:
        return bswap(0x08030100);
    case 77:
        return bswap(0x08040200);
    case 78:
        return bswap(0x08030200);
    case 79:
        return bswap(0x08040100);
    case 58:
        return bswap(0x08000001);
    case 29: {
        KeyAction a;
        a.key = 0x49;
        return a.encode();
    } // Insert
    case 37: {
        KeyAction a;
        a.key = 0x46;
        return a.encode();
    } // Print Screen
    case 38: {
        KeyAction a;
        a.key = 0x47;
        return a.encode();
    } // Scroll Lock
    case 39: {
        KeyAction a;
        a.key = 0x48;
        return a.encode();
    } // Pause
    case 14: {
        KeyAction a;
        a.key = 0x4A;
        return a.encode();
    } // Home
    }
    return 0;
}

uint32_t baseDefault(const KeyInfo& key) {
    if(key.isFn)
        return 0x0000000d;
    if(key.isKnob)
        return bswap(0x0700001D);
    KeyAction a;
    if(key.usage >= 0xE0 && key.usage <= 0xE7)
        a.modifiers = uint8_t(1 << (key.usage - 0xE0)); // modifier keys are stored as their bit, not a usage
    else
        a.key = key.usage;
    return a.encode();
}

uint32_t defaultWord(int layer, const KeyInfo& key) {
    // The keyboard stores 0 (no Fn function) for Fn-layer keys without a factory override.
    return layer == fnLayer ? fnDefault(key) : baseDefault(key);
}

QVariantList macrosToVariant(const QList<Macro>& macros) {
    QVariantList list;
    for(const Macro& m : macros) {
        QVariantList events;
        for(const MacroEvent& e : m.events)
            events.append(QVariantMap{{QStringLiteral("kind"), int(e.kind)},
                                      {QStringLiteral("code"), e.code},
                                      {QStringLiteral("release"), e.release},
                                      {QStringLiteral("delayMs"), e.delayMs}});
        list.append(QVariantMap{{QStringLiteral("name"), m.name}, {QStringLiteral("events"), events}});
    }
    return list;
}

QList<Macro> macrosFromVariant(const QVariantList& list) {
    QList<Macro> macros;
    for(const QVariant& v : list) {
        const QVariantMap m = v.toMap();
        Macro macro;
        macro.name = m.value(QStringLiteral("name")).toString().left(macroNameMaxChars);
        for(const QVariant& ev : m.value(QStringLiteral("events")).toList()) {
            const QVariantMap e = ev.toMap();
            MacroEvent event;
            event.kind = static_cast<MacroEventKind>(std::clamp(e.value(QStringLiteral("kind")).toInt(), 0, 5));
            event.code = static_cast<uint8_t>(e.value(QStringLiteral("code")).toInt());
            event.release = e.value(QStringLiteral("release")).toBool();
            event.delayMs = std::clamp(e.value(QStringLiteral("delayMs")).toInt(), 0, 0xfffff);
            macro.events.append(event);
        }
        macros.append(macro);
    }
    return macros;
}

QList<KeyAction> layerActions(int layer, const QVariantMap& ledToCode) {
    QList<KeyAction> actions(matrixLength, KeyAction{});
    for(KeyAction& a : actions)
        a.disabled = true;
    for(const KeyInfo& key : keyLayout())
        actions[key.ledIndex] = KeyAction::decode(defaultWord(layer, key));
    for(auto it = ledToCode.begin(); it != ledToCode.end(); ++it) {
        bool ok = false;
        const int led = it.key().toInt(&ok);
        if(ok && led >= 0 && led < matrixLength)
            actions[led] = KeyAction::decode(it.value().toUInt());
    }
    return actions;
}

QList<QColor> frameFromMap(const QVariantMap& ledToColor) {
    QList<QColor> frame(matrixLength, QColor());
    for(auto it = ledToColor.begin(); it != ledToColor.end(); ++it) {
        bool ok = false;
        const int led = it.key().toInt(&ok);
        if(ok && led >= 0 && led < matrixLength)
            frame[led] = QColor(it.value().toString());
    }
    return frame;
}
} // namespace

DeviceController::DeviceController(QObject* parent) : QObject(parent) {
    connect(&m_watcher, &QFutureWatcher<Outcome>::finished, this, &DeviceController::finish);
}

DeviceController::~DeviceController() {
    stopStream();
    stopReceiverMonitor();
    m_watcher.waitForFinished();
}

QVariantMap DeviceController::receiverStatus() const {
    QMutexLocker lock(&m_receiverMutex);
    return m_receiver;
}

QVariantList DeviceController::effects() const {
    QVariantList list;
    for(const Effect& e : F75::effects())
        list.append(QVariantMap{{QStringLiteral("index"), e.index},
                                {QStringLiteral("hardwareId"), e.hardwareId},
                                {QStringLiteral("name"), QLatin1String(e.name)},
                                {QStringLiteral("hasSpeed"), e.hasSpeed},
                                {QStringLiteral("hasBrightness"), e.hasBrightness},
                                {QStringLiteral("hasColor"), e.hasColor}});
    return list;
}

QVariantList DeviceController::colors() const {
    QVariantList list;
    const QStringList names = colorNames();
    for(int i = 0; i < names.size(); ++i)
        list.append(QVariantMap{{QStringLiteral("name"), names[i]}, {QStringLiteral("color"), presetColor(i).name()}});
    return list;
}

QVariantList DeviceController::sleepPresets() const {
    QVariantList list;
    for(const SleepPreset& p : F75::sleepPresets())
        list.append(QVariantMap{{QStringLiteral("seconds"), p.seconds}, {QStringLiteral("hardware"), p.hardware}});
    return list;
}

QVariantList DeviceController::keys() const {
    QVariantList list;
    for(const KeyInfo& k : keyLayout())
        list.append(QVariantMap{{QStringLiteral("id"), k.id},
                                {QStringLiteral("x"), k.rect.x()},
                                {QStringLiteral("y"), k.rect.y()},
                                {QStringLiteral("w"), k.rect.width()},
                                {QStringLiteral("h"), k.rect.height()},
                                {QStringLiteral("usage"), k.usage},
                                {QStringLiteral("led"), k.ledIndex},
                                {QStringLiteral("label"), QString::fromUtf8(k.label)},
                                {QStringLiteral("isFn"), k.isFn},
                                {QStringLiteral("isKnob"), k.isKnob}});
    return list;
}

QVariantList DeviceController::hidUsages() const {
    QVariantList list;
    for(const auto& [usage, name] : F75::hidUsages())
        list.append(QVariantMap{{QStringLiteral("usage"), usage}, {QStringLiteral("name"), name}});
    return list;
}

QVariantList DeviceController::consumerUsages() const {
    QVariantList list;
    for(const auto& [usage, name] : F75::consumerUsages())
        list.append(QVariantMap{{QStringLiteral("usage"), usage}, {QStringLiteral("name"), name}});
    return list;
}

QVariantList DeviceController::deviceFunctions() const {
    // Every distinct device-function word the factory Fn layer uses, plus the mode switches.
    static const uint32_t words[] = {0x07000004, 0x08030100, 0x08030200, 0x08040100, 0x08040200, 0x07000005, 0x07000006,
                                     0x07000007, 0x08020000, 0x08000000, 0x07000008, 0x0700000a, 0x07000011, 0x07000019,
                                     0x07000018, 0x0700001A, 0x07000001, 0x07000002, 0x08000001};
    QVariantList list;
    for(uint32_t w : words) {
        const uint32_t code = bswap(w);
        list.append(QVariantMap{{QStringLiteral("code"), code}, {QStringLiteral("name"), describeAction(code)}});
    }
    return list;
}

DeviceController::Outcome DeviceController::runProbe() {
    Outcome outcome;
    Session session;
    if(const auto verified = verify(session, outcome)) {
        outcome.state = QStringLiteral("verified");
        outcome.message =
            QStringLiteral("%1 answered with a valid identity and config block.").arg(verified->variant.name);
        outcome.current = describe(verified->config, verified->variant);
    }
    outcome.log = session.log;
    return outcome;
}

void DeviceController::probe() {
    start([] { return runProbe(); });
}

QJsonObject DeviceController::runMatrixTest() {
    QJsonObject out;
    Session session;
    Outcome outcome;
    const auto verified = verify(session, outcome);
    if(!verified) {
        out[QStringLiteral("state")] = outcome.state;
        out[QStringLiteral("message")] = outcome.message;
        return out;
    }
    const auto original = session.read(Command::Matrix, fnLayer, matrixBytes);
    if(!original || original->size() != matrixBytes) {
        out[QStringLiteral("state")] = QStringLiteral("error");
        out[QStringLiteral("message")] = QStringLiteral("Could not read layer 1: %1").arg(session.error);
        return out;
    }
    const KeyInfo* oKey = nullptr;
    for(const KeyInfo& k : keyLayout())
        if(k.id == 39)
            oKey = &k; // O
    const int at = oKey->ledIndex * 4;
    const uint32_t before = qFromLittleEndian<quint32>(original->constData() + at);
    KeyAction insert;
    insert.key = 0x49;
    QByteArray modified = *original;
    qToLittleEndian<quint32>(insert.encode(), modified.data() + at);
    out[QStringLiteral("before")] =
        QStringLiteral("0x%1 %2").arg(before, 8, 16, QLatin1Char('0')).arg(KeyAction::decode(before).describe());
    auto step = [&](const QByteArray& bytes, const char* name) {
        if(!session.write(Command::Matrix, fnLayer, bytes)) {
            out[QLatin1String(name)] = QStringLiteral("write failed: %1").arg(session.error);
            return false;
        }
        QThread::msleep(100);
        const auto back = session.read(Command::Matrix, fnLayer, matrixBytes);
        const uint32_t word = back ? qFromLittleEndian<quint32>(back->constData() + at) : 0;
        out[QLatin1String(name)] =
            QStringLiteral("readback %1, Fn+O = 0x%2 %3")
                .arg(back && *back == bytes ? QStringLiteral("identical") : QStringLiteral("DIFFERENT"))
                .arg(word, 8, 16, QLatin1Char('0'))
                .arg(KeyAction::decode(word).describe());
        return back && *back == bytes;
    };
    const bool changed = step(modified, "changed");
    const bool restored = step(*original, "restored");
    out[QStringLiteral("state")] = changed && restored ? QStringLiteral("verified") : QStringLiteral("error");
    out[QStringLiteral("message")] = changed && restored
                                         ? QStringLiteral("Layer write confirmed: Fn+O changed to Insert and restored.")
                                         : QStringLiteral("Layer write did not round-trip; see steps.");
    out[QStringLiteral("log")] = QJsonArray::fromStringList(session.log);
    return out;
}

QJsonObject DeviceController::runStreamTest() {
    QJsonObject out;
    Session session;
    Outcome outcome;
    const auto verified = verify(session, outcome);
    if(!verified) {
        out[QStringLiteral("state")] = outcome.state;
        out[QStringLiteral("message")] = outcome.message;
        return out;
    }
    const QByteArray header = encodePages(Command::Realtime, 0, QByteArray(realtimeFrameBytes, '\0')).first().left(8);
    int sent = 0;
    QString error;
    for(int i = 0; i < 30; ++i) {
        QList<QColor> frame(matrixLength, QColor());
        for(const KeyInfo& key : keyLayout())
            frame[key.ledIndex] =
                QColor::fromHsvF(float(std::fmod(i / 30.0 + key.rect.center().x() / 660.0, 1.0)), 1.0f, 1.0f);
        QByteArray report = header + encodeRealtimeFrame(frame);
        report.resize(reportLength);
        QThread::msleep(realtimePreDelayMs);
        if(!session.transport->setFeature(report, error))
            break;
        ++sent;
        QThread::msleep(93);
    }
    out[QStringLiteral("framesSent")] = sent;
    out[QStringLiteral("sendError")] = error;
    QThread::msleep(300);
    Session again;
    Outcome after;
    const bool alive = verify(again, after).has_value();
    out[QStringLiteral("state")] = alive ? QStringLiteral("verified") : after.state;
    out[QStringLiteral("message")] =
        alive ? QStringLiteral("Keyboard still answers after %1 real-time frames.").arg(sent) : after.message;
    out[QStringLiteral("log")] = QJsonArray::fromStringList(session.log.mid(0, 6) + again.log);
    return out;
}

QJsonObject DeviceController::runDump() {
    QJsonObject out;
    Session session;
    Outcome outcome;
    const auto verified = verify(session, outcome);
    out[QStringLiteral("state")] = verified ? QStringLiteral("verified") : outcome.state;
    out[QStringLiteral("message")] = outcome.message;
    if(verified && qEnvironmentVariableIsSet("F75_REAPPLY")) {
        // Writes the config block back unchanged and reads it again: exercises the write path without altering
        // settings.
        writeConfig(session, outcome, *verified, verified->config.raw, QStringLiteral("Re-apply"));
        out[QStringLiteral("reapply")] = outcome.message;
        out[QStringLiteral("state")] = outcome.state;
    }
    if(verified) {
        QJsonObject layers;
        for(int layer = 0; layer < 2; ++layer) {
            const auto bytes = session.read(Command::Matrix, static_cast<uint8_t>(layer), matrixBytes);
            if(!bytes) {
                out[QStringLiteral("matrixError")] = session.error;
                break;
            }
            QJsonObject entries;
            const auto actions = decodeMatrixLayer(*bytes);
            for(const KeyInfo& key : keyLayout()) {
                const KeyAction& a = actions[key.ledIndex];
                const uint32_t expected = defaultWord(layer, key);
                entries[QString::fromUtf8(key.label).isEmpty() ? QStringLiteral("Space")
                                                               : QString::fromUtf8(key.label)] =
                    QStringLiteral("0x%1 %2%3")
                        .arg(a.encode(), 8, 16, QLatin1Char('0'))
                        .arg(a.describe(),
                             a.encode() == expected
                                 ? QString()
                                 : QStringLiteral("  [expected 0x%1]").arg(expected, 8, 16, QLatin1Char('0')));
            }
            entries[QStringLiteral("_tail")] = hex(bytes->right(8));
            layers[QString::number(layer)] = entries;
        }
        out[QStringLiteral("layers")] = layers;
        const auto macroBytes = session.read(Command::Macro, 0, 0x200);
        if(macroBytes) {
            out[QStringLiteral("macroHead")] = hex(macroBytes->left(48));
            out[QStringLiteral("macros")] =
                QJsonArray::fromVariantList(macrosToVariant(decodeMacroBuffer(*macroBytes)));
        }
    }
    out[QStringLiteral("log")] = QJsonArray::fromStringList(session.log);
    return out;
}

void DeviceController::readKeymap(int layer) {
    if(!verified() || layer < 0 || layer >= layerCount)
        return;
    start([layer] {
        Outcome outcome;
        Session session;
        const auto verified = verify(session, outcome);
        if(!verified) {
            outcome.log = session.log;
            return outcome;
        }
        const auto bytes = session.read(Command::Matrix, static_cast<uint8_t>(layer), matrixBytes);
        if(!bytes) {
            outcome.state = QStringLiteral("error");
            outcome.message = QStringLiteral("Matrix read failed: %1").arg(session.error);
        } else {
            QVariantMap entries;
            const auto actions = decodeMatrixLayer(*bytes);
            for(int i = 0; i < actions.size(); ++i)
                if(!actions[i].disabled)
                    entries.insert(QString::number(i), actions[i].encode());
            outcome.keymapLayer = {{QStringLiteral("layer"), layer}, {QStringLiteral("entries"), entries}};
            outcome.state = QStringLiteral("verified");
            outcome.message = QStringLiteral("Read key layer %1 (%2 assigned keys).").arg(layer).arg(entries.size());
            outcome.current = describe(verified->config, verified->variant);
        }
        outcome.log = session.log;
        return outcome;
    });
}

void DeviceController::readMacros() {
    if(!verified())
        return;
    start([] {
        Outcome outcome;
        Session session;
        const auto verified = verify(session, outcome);
        if(!verified) {
            outcome.log = session.log;
            return outcome;
        }
        const auto bytes = session.read(Command::Macro, 0, 0x1000);
        if(!bytes) {
            outcome.state = QStringLiteral("error");
            outcome.message = QStringLiteral("Macro read failed: %1").arg(session.error);
        } else {
            outcome.macros = macrosToVariant(decodeMacroBuffer(*bytes));
            outcome.state = QStringLiteral("verified");
            outcome.message = QStringLiteral("Read %1 macro(s) from the keyboard.").arg(outcome.macros.size());
            outcome.current = describe(verified->config, verified->variant);
        }
        outcome.log = session.log;
        return outcome;
    });
}

void DeviceController::applyEffect(int effectIndex, int brightness, int speed, int color) {
    if(!verified())
        return;
    start([=] {
        Outcome outcome;
        Session session;
        const auto verified = verify(session, outcome);
        if(!verified) {
            outcome.log = session.log;
            return outcome;
        }
        const QByteArray next = encodeEffect(verified->config.raw, effectIndex, {brightness, speed, color});
        if(next.isEmpty()) {
            outcome.state = QStringLiteral("error");
            outcome.message = QStringLiteral("Invalid effect selection.");
        } else {
            writeConfig(session, outcome, *verified, next, QStringLiteral("Lighting"));
        }
        outcome.log = session.log;
        return outcome;
    });
}

void DeviceController::applySleep(int seconds) {
    if(!verified())
        return;
    std::optional<uint8_t> hardware;
    for(const SleepPreset& p : F75::sleepPresets())
        if(p.seconds == seconds)
            hardware = p.hardware;
    if(!hardware)
        return;
    const uint8_t value = *hardware;
    start([=] {
        Outcome outcome;
        Session session;
        const auto verified = verify(session, outcome);
        if(!verified) {
            outcome.log = session.log;
            return outcome;
        }
        writeConfig(session, outcome, *verified, encodeSleep(verified->config.raw, value),
                    QStringLiteral("Sleep timer"));
        outcome.log = session.log;
        return outcome;
    });
}

void DeviceController::applyPerKey(const QVariantMap& ledToColor) {
    if(!verified())
        return;
    const QList<QColor> frame = frameFromMap(ledToColor);
    start([=] {
        Outcome outcome;
        Session session;
        const auto verified = verify(session, outcome);
        if(!verified) {
            outcome.log = session.log;
            return outcome;
        }
        outcome.wrote = true;
        if(!session.write(Command::ColorGroup, 0, encodeColorGroup(frame))) {
            outcome.state = QStringLiteral("error");
            outcome.message = QStringLiteral("Per-key colour write failed: %1").arg(session.error);
            outcome.log = session.log;
            return outcome;
        }
        const SlotSettings slot = verified->config.slot[customEffectIndex];
        const QByteArray next = encodeEffect(verified->config.raw, customEffectIndex, slot);
        if(writeConfig(session, outcome, *verified, next, QStringLiteral("Per-key lighting")))
            outcome.message += QStringLiteral(" Colour group 0 uploaded (%1 keys).").arg(ledToColor.size());
        outcome.log = session.log;
        return outcome;
    });
}

void DeviceController::applyKeymap(int layer, const QVariantMap& ledToCode) {
    if(!verified() || layer < 0 || layer >= layerCount)
        return;
    const QList<KeyAction> actions = layerActions(layer, ledToCode);
    start([=] {
        Outcome outcome;
        Session session;
        const auto verified = verify(session, outcome);
        if(!verified) {
            outcome.log = session.log;
            return outcome;
        }
        outcome.wrote = true;
        // Each layer is terminated with the A5 5A marker word, as the keyboard returns it.
        const QByteArray bytes = encodeMatrixLayer(actions, true);
        if(!session.write(Command::Matrix, static_cast<uint8_t>(layer), bytes)) {
            outcome.state = QStringLiteral("error");
            outcome.message = QStringLiteral("Key layer %1 write failed: %2").arg(layer).arg(session.error);
            outcome.log = session.log;
            return outcome;
        }
        const auto readback = session.read(Command::Matrix, static_cast<uint8_t>(layer), matrixBytes);
        QVariantMap entries;
        if(readback) {
            const auto decoded = decodeMatrixLayer(*readback);
            for(int i = 0; i < decoded.size(); ++i)
                if(!decoded[i].disabled)
                    entries.insert(QString::number(i), decoded[i].encode());
        }
        outcome.keymapLayer = {{QStringLiteral("layer"), layer}, {QStringLiteral("entries"), entries}};
        outcome.state = QStringLiteral("verified");
        outcome.message =
            !readback ? QStringLiteral("Key layer %1 sent, but readback failed: %2").arg(layer).arg(session.error)
            : *readback == bytes
                ? QStringLiteral("Key layer %1 applied and confirmed by readback.").arg(layer)
                : QStringLiteral("Key layer %1 sent; the keyboard reports a different layer, showing what it holds.")
                      .arg(layer);
        outcome.current = describe(verified->config, verified->variant);
        outcome.log = session.log;
        return outcome;
    });
}

void DeviceController::applyMacros(const QVariantList& macros) {
    if(!verified())
        return;
    const QByteArray buffer = encodeMacroBuffer(macrosFromVariant(macros));
    if(buffer.isEmpty() || buffer.size() > macroBufferMax)
        return;
    start([=] {
        Outcome outcome;
        Session session;
        const auto verified = verify(session, outcome);
        if(!verified) {
            outcome.log = session.log;
            return outcome;
        }
        outcome.wrote = true;
        if(!session.write(Command::Macro, 0, buffer)) {
            outcome.state = QStringLiteral("error");
            outcome.message = QStringLiteral("Macro upload failed: %1").arg(session.error);
        } else {
            const auto readback = session.read(Command::Macro, 0, static_cast<int>(buffer.size()));
            outcome.macros = readback ? macrosToVariant(decodeMacroBuffer(*readback)) : macros;
            outcome.state = QStringLiteral("verified");
            outcome.message =
                readback && *readback == buffer
                    ? QStringLiteral("Macros uploaded and confirmed by readback.")
                    : QStringLiteral("Macros uploaded (%1 bytes); readback differed or failed.").arg(buffer.size());
            outcome.current = describe(verified->config, verified->variant);
        }
        outcome.log = session.log;
        return outcome;
    });
}

void DeviceController::applyProfile(const QVariantMap& profile) {
    if(!verified())
        return;
    const QVariantMap lighting = profile.value(QStringLiteral("lighting")).toMap();
    const int effectIndex = lighting.value(QStringLiteral("effectIndex")).toInt();
    const SlotSettings slot{lighting.value(QStringLiteral("brightness")).toInt(),
                            lighting.value(QStringLiteral("speed")).toInt(),
                            lighting.value(QStringLiteral("color")).toInt()};
    const QList<QColor> frame = frameFromMap(profile.value(QStringLiteral("perKey")).toMap());
    const QVariantMap keymap = profile.value(QStringLiteral("keymap")).toMap();
    const QList<Macro> macros = macrosFromVariant(profile.value(QStringLiteral("macros")).toList());
    const int sleepSeconds = profile.value(QStringLiteral("sleepSeconds")).toInt();
    start([=] {
        Outcome outcome;
        Session session;
        const auto verified = verify(session, outcome);
        if(!verified) {
            outcome.log = session.log;
            return outcome;
        }
        outcome.wrote = true;
        QStringList done;
        if(!macros.isEmpty()) {
            if(!session.write(Command::Macro, 0, encodeMacroBuffer(macros))) {
                outcome.state = QStringLiteral("error");
                outcome.message = QStringLiteral("Macro upload failed: %1").arg(session.error);
                outcome.log = session.log;
                return outcome;
            }
            done << QStringLiteral("%1 macros").arg(macros.size());
        }
        for(auto it = keymap.begin(); it != keymap.end(); ++it) {
            const int layer = it.key().toInt();
            if(layer < 0 || layer >= layerCount || it.value().toMap().isEmpty())
                continue;
            if(!session.write(Command::Matrix, static_cast<uint8_t>(layer),
                              encodeMatrixLayer(layerActions(layer, it.value().toMap()), true))) {
                outcome.state = QStringLiteral("error");
                outcome.message = QStringLiteral("Key layer %1 write failed: %2").arg(layer).arg(session.error);
                outcome.log = session.log;
                return outcome;
            }
            done << QStringLiteral("layer %1").arg(layer);
        }
        if(effectIndex == customEffectIndex) {
            if(!session.write(Command::ColorGroup, 0, encodeColorGroup(frame))) {
                outcome.state = QStringLiteral("error");
                outcome.message = QStringLiteral("Per-key colour write failed: %1").arg(session.error);
                outcome.log = session.log;
                return outcome;
            }
            done << QStringLiteral("per-key colours");
        }
        QByteArray next = encodeEffect(verified->config.raw, effectIndex, slot);
        for(const SleepPreset& p : F75::sleepPresets())
            if(p.seconds == sleepSeconds)
                next = encodeSleep(next, p.hardware);
        done << QStringLiteral("lighting + sleep");
        if(writeConfig(session, outcome, *verified, next, QStringLiteral("Profile")))
            outcome.message =
                QStringLiteral("Profile \"%1\" applied: %2.")
                    .arg(profile.value(QStringLiteral("name")).toString(), done.join(QStringLiteral(", ")));
        outcome.log = session.log;
        return outcome;
    });
}

void DeviceController::factoryReset() {
    if(!verified())
        return;
    start([] {
        Outcome outcome;
        Session session;
        const auto verified = verify(session, outcome);
        if(!verified) {
            outcome.log = session.log;
            return outcome;
        }
        outcome.wrote = true;
        if(!session.write(Command::FactoryReset, 0, QByteArray(1, '\0'))) {
            outcome.state = QStringLiteral("error");
            outcome.message = QStringLiteral("Reset command failed: %1").arg(session.error);
            outcome.log = session.log;
            return outcome;
        }
        QThread::msleep(600);
        Session again;
        Outcome after;
        if(const auto v = verify(again, after)) {
            outcome.state = QStringLiteral("verified");
            outcome.message = QStringLiteral("Factory reset sent; keyboard answered afterwards.");
            outcome.current = describe(v->config, v->variant);
            outcome.link = after.link;
        } else {
            outcome.state = after.state;
            outcome.link = after.link;
            outcome.message =
                QStringLiteral("Reset sent, but the keyboard did not answer afterwards: %1").arg(after.message);
        }
        outcome.log = session.log + again.log;
        return outcome;
    });
}

void DeviceController::start(std::function<Outcome()> job) {
    if(busy())
        return;
    m_state = QStringLiteral("busy");
    m_message = QStringLiteral("Talking to the keyboard…");
    emit changed();
    m_watcher.setFuture(QtConcurrent::run(std::move(job)));
}

void DeviceController::finish() {
    const Outcome outcome = m_watcher.result();
    TRC_INFO("device job done: state={} link={} wrote={} \"{}\"", outcome.state.toStdString(),
             outcome.link.toStdString(), outcome.wrote, outcome.message.toStdString());
    m_state = outcome.state;
    m_message = outcome.message;
    if(!outcome.link.isEmpty())
        m_link = outcome.link;
    if(!outcome.current.isEmpty())
        m_current = outcome.current;
    if(!outcome.keymapLayer.isEmpty())
        m_keymap.insert(outcome.keymapLayer.value(QStringLiteral("layer")).toString(),
                        outcome.keymapLayer.value(QStringLiteral("entries")));
    if(!outcome.macros.isEmpty() || outcome.message.contains(QStringLiteral("macro")))
        m_macros = outcome.macros;
    for(const QString& line : outcome.log)
        m_log.append(line);
    while(m_log.size() > 120)
        m_log.removeFirst();
    emit changed();
}

// ---- host-streamed per-key lighting --------------------------------------

void DeviceController::startStream(HostEffectsController* source) {
    if(!verified() || m_streaming || !source)
        return;
    m_source = source;
    m_streamStop = false;
    m_streaming = true;
    connect(source, &HostEffectsController::frameReady, this, &DeviceController::onFrame, Qt::UniqueConnection);
    m_streamThread = std::thread([this] { streamLoop(); });
    m_message = QStringLiteral("Streaming host lighting to the keyboard…");
    m_state = QStringLiteral("verified");
    emit changed();
}

void DeviceController::stopStream() {
    if(!m_streaming && !m_streamThread.joinable())
        return;
    m_streamStop = true;
    {
        QMutexLocker lock(&m_frameMutex);
        m_frameReady.wakeAll();
    }
    if(m_streamThread.joinable())
        m_streamThread.join();
    if(m_source)
        disconnect(m_source, &HostEffectsController::frameReady, this, &DeviceController::onFrame);
    m_source = nullptr;
    const bool wasStreaming = m_streaming.exchange(false);
    if(wasStreaming) {
        m_message =
            QStringLiteral("Host lighting stopped. The keyboard returns to its own effect once frames stop arriving.");
        emit changed();
    }
}

void DeviceController::onFrame(QList<QColor> frame) {
    QMutexLocker lock(&m_frameMutex);
    m_pendingFrame = std::move(frame);
    m_hasFrame = true;
    m_frameReady.wakeOne();
}

void DeviceController::streamLoop() {
    Session session;
    Outcome outcome;
    const auto verified = verify(session, outcome);
    if(!verified) {
        QMetaObject::invokeMethod(
            this,
            [this, msg = outcome.message] {
                m_streaming = false;
                m_state = QStringLiteral("error");
                m_message = QStringLiteral("Streaming could not start: %1").arg(msg);
                emit changed();
            },
            Qt::QueuedConnection);
        return;
    }
    // Real-time frames (command 0x08) are transient: no config or
    // colour-group write, so nothing is persisted and the keyboard is never asked to flash per frame.
    const QByteArray frameHeader =
        encodePages(Command::Realtime, 0, QByteArray(realtimeFrameBytes, '\0')).first().left(8);
    int frames = 0, failures = 0;
    QElapsedTimer pacing;
    pacing.start();
    while(!m_streamStop) {
        QList<QColor> frame;
        {
            QMutexLocker lock(&m_frameMutex);
            if(!m_hasFrame)
                m_frameReady.wait(&m_frameMutex, 250);
            if(!m_hasFrame)
                continue;
            frame = m_pendingFrame;
            m_hasFrame = false;
        }
        // Never faster than 20 frames/s regardless of the source rate.
        const qint64 since = pacing.elapsed();
        if(since < 50)
            QThread::msleep(static_cast<unsigned long>(50 - since));
        pacing.restart();
        QString error;
        QByteArray report = frameHeader + encodeRealtimeFrame(frame);
        report.resize(reportLength);
        QThread::msleep(realtimePreDelayMs);
        if(!session.transport->setFeature(report, error)) {
            if(++failures >= 5) {
                QMetaObject::invokeMethod(
                    this,
                    [this, error] {
                        m_streaming = false;
                        m_state = QStringLiteral("error");
                        m_message = QStringLiteral("Streaming stopped: %1").arg(error);
                        emit changed();
                    },
                    Qt::QueuedConnection);
                return;
            }
            QThread::msleep(100);
            continue;
        }
        failures = 0;
        if(++frames % 60 == 0)
            QMetaObject::invokeMethod(
                this,
                [this, frames] {
                    m_message = QStringLiteral("Streaming host lighting… %1 frames sent.").arg(frames);
                    emit changed();
                },
                Qt::QueuedConnection);
    }
}

// ---- hot-plug and interface listing ---------------------------------------

QVariantList DeviceController::hidInterfaces() const {
    QVariantList rows;
    QString error;
    const auto role = [](const HidTransport::Match& m) {
        if(m.vendorId == usbVendorId && m.usagePage == commandUsagePage && m.usage == commandUsage)
            return m.featureReportLength == reportLength ? QStringLiteral("USB command") : QStringLiteral("Vendor");
        if(m.vendorId == receiverVendorId && m.usagePage == receiverStatusUsagePage && m.usage == receiverStatusUsage)
            return QStringLiteral("Receiver status");
        if(m.usagePage == 0x0001 && m.usage == 0x0006)
            return QStringLiteral("Keyboard");
        if(m.usagePage == 0x0001 && m.usage == 0x0002)
            return QStringLiteral("Mouse");
        if(m.usagePage == 0x000C)
            return QStringLiteral("Media");
        if(m.usagePage == 0x0001 && m.usage == 0x0080)
            return QStringLiteral("System control");
        return QStringLiteral("HID");
    };
    for(const auto& [vid, pid] :
        {std::pair{usbVendorId, usbProductId}, std::pair{receiverVendorId, receiverProductId}}) {
        for(const auto& m : HidTransport::enumerate(vid, pid, error)) {
            rows.append(QVariantMap{
                {QStringLiteral("link"), vid == usbVendorId ? QStringLiteral("usb") : QStringLiteral("receiver")},
                {QStringLiteral("role"), role(m)},
                {QStringLiteral("vid"), QStringLiteral("%1").arg(m.vendorId, 4, 16, QLatin1Char('0')).toUpper()},
                {QStringLiteral("pid"), QStringLiteral("%1").arg(m.productId, 4, 16, QLatin1Char('0')).toUpper()},
                {QStringLiteral("usagePage"), QStringLiteral("%1").arg(m.usagePage, 4, 16, QLatin1Char('0')).toUpper()},
                {QStringLiteral("usage"), QStringLiteral("%1").arg(m.usage, 4, 16, QLatin1Char('0')).toUpper()},
                {QStringLiteral("feature"), m.featureReportLength},
                {QStringLiteral("input"), m.inputReportLength},
                {QStringLiteral("output"), m.outputReportLength},
                {QStringLiteral("path"), m.devicePath}});
        }
    }
    if(!error.isEmpty())
        TRC_WARN("hid enumeration: {}", error.toStdString());
    return rows;
}

void DeviceController::handleUsbArrived() {
    TRC_INFO("usb arrived; probing");
    if(busy())
        return;
    probe();
}

void DeviceController::handleUsbRemoved() {
    TRC_INFO("usb removed");
    if(m_streaming)
        stopStream();
    if(busy())
        return;
    m_state = QStringLiteral("absent");
    m_link = QStringLiteral("none");
    m_message = QStringLiteral("Keyboard unplugged. Connect the USB cable to configure it.");
    emit changed();
}

// ---- receiver status channel ---------------------------------------------

void DeviceController::startReceiverMonitor() {
    if(m_monitorThread.joinable())
        return;
    m_monitorStop = false;
    m_monitorThread = std::thread([this] { monitorLoop(); });
}

void DeviceController::stopReceiverMonitor() {
    m_monitorStop = true;
    if(m_monitorThread.joinable())
        m_monitorThread.join();
}

void DeviceController::monitorLoop() {
    // Observes the receiver's FF02/0002 20-byte input reports (wireless status
    // channel). The payload layout is not decoded yet; reports are shown raw.
    QString error;
    std::unique_ptr<HidTransport> transport;
    int count = 0;
    auto publish = [&](const QVariantMap& status) {
        {
            QMutexLocker lock(&m_receiverMutex);
            m_receiver = status;
        }
        QMetaObject::invokeMethod(this, [this] { emit receiverChanged(); }, Qt::QueuedConnection);
    };
    while(!m_monitorStop) {
        if(!transport) {
            transport = HidTransport::open({receiverVendorId, receiverProductId, receiverStatusUsagePage,
                                            receiverStatusUsage, -1, receiverStatusLength, true},
                                           error);
            if(!transport) {
                publish({{QStringLiteral("present"), false},
                         {QStringLiteral("message"), QStringLiteral("Receiver status channel not present.")}});
                for(int i = 0; i < 20 && !m_monitorStop; ++i)
                    QThread::msleep(100);
                continue;
            }
            publish({{QStringLiteral("present"), true},
                     {QStringLiteral("count"), 0},
                     {QStringLiteral("message"),
                      QStringLiteral("Listening on the receiver status channel (FF02/0002, 20-byte reports).")}});
        }
        const auto report = transport->readInput(500, error);
        if(!report) {
            if(!error.isEmpty()) {
                transport.reset();
                publish({{QStringLiteral("present"), false},
                         {QStringLiteral("message"), QStringLiteral("Receiver read failed: %1").arg(error)}});
                error.clear();
            }
            continue;
        }
        ++count;
        publish({{QStringLiteral("present"), true},
                 {QStringLiteral("count"), count},
                 {QStringLiteral("lastHex"), hex(*report)},
                 {QStringLiteral("lastAt"), QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss"))},
                 {QStringLiteral("message"), QStringLiteral("Status report received; layout not yet decoded.")}});
    }
}

// ---- key action helpers ---------------------------------------------------

quint32 DeviceController::encodeKeyboard(int modifiers, int usage, int usage2) const {
    KeyAction a;
    a.modifiers = static_cast<uint8_t>(modifiers);
    a.key = static_cast<uint8_t>(usage);
    a.key2 = static_cast<uint8_t>(usage2);
    return a.encode();
}

quint32 DeviceController::encodeConsumer(int usage) const {
    KeyAction a;
    a.kind = ActionKind::Consumer;
    a.usage = static_cast<uint16_t>(usage);
    return a.encode();
}

quint32 DeviceController::encodeMacro(int macroIndex, int mode, int repeat) const {
    KeyAction a;
    a.kind = ActionKind::Macro;
    a.key = static_cast<uint8_t>(mode);
    a.key2 = static_cast<uint8_t>(std::clamp(repeat, 1, 255));
    a.macroIndex = static_cast<uint8_t>(std::clamp(macroIndex, 0, macroMaxCount - 1));
    return a.encode();
}

quint32 DeviceController::encodeMouse(int button) const {
    KeyAction a;
    a.kind = ActionKind::Mouse;
    a.key = static_cast<uint8_t>(button);
    return a.encode();
}

quint32 DeviceController::encodeProfile(int profileIndex) const {
    KeyAction a;
    a.kind = ActionKind::Device;
    a.key = 5;
    a.key2 = static_cast<uint8_t>(std::clamp(profileIndex, 0, 3));
    return a.encode();
}

QString DeviceController::describeAction(quint32 code) const {
    const KeyAction a = KeyAction::decode(code);
    const uint32_t be = bswap(code);
    const uint8_t type = (be >> 24) & 0xff;
    if(code == 0x0d)
        return QStringLiteral("Fn");
    if(type == 0x07) {
        // Factory Fn-layer words observed on the keyboard; names inferred from the F75 manual layout.
        switch(be & 0xffffff) {
        case 0x01:
            return QStringLiteral("Left Click");
        case 0x02:
            return QStringLiteral("Right Click");
        case 0x04:
            return QStringLiteral("Lighting on/off");
        case 0x05:
        case 0x06:
        case 0x07:
            return QStringLiteral("Custom light %1").arg((be & 0xff) - 4);
        case 0x08:
            return QStringLiteral("Wired mode");
        case 0x0a:
            return QStringLiteral("2.4 GHz mode");
        case 0x11:
            return QStringLiteral("Bluetooth pairing");
        case 0x19:
            return QStringLiteral("Bluetooth 1");
        case 0x18:
            return QStringLiteral("Bluetooth 2");
        case 0x1a:
            return QStringLiteral("Bluetooth 3");
        case 0x1d:
            return QStringLiteral("Knob");
        }
        return QStringLiteral("Device fn 0x%1").arg(be & 0xffffff, 6, 16, QLatin1Char('0'));
    }
    if(type == 0x08) {
        switch(be & 0xffffff) {
        case 0x030100:
            return QStringLiteral("Brightness +");
        case 0x030200:
            return QStringLiteral("Brightness −");
        case 0x040100:
            return QStringLiteral("Effect speed +");
        case 0x040200:
            return QStringLiteral("Effect speed −");
        case 0x020000:
            return QStringLiteral("Win lock");
        case 0x000000:
            return QStringLiteral("Keyboard lock");
        case 0x000001:
            return QStringLiteral("Fn shift");
        }
        return QStringLiteral("Device fn 0x%1").arg(be & 0xffffff, 6, 16, QLatin1Char('0'));
    }
    return a.describe();
}

QVariantMap DeviceController::decodeAction(quint32 code) const {
    const KeyAction a = KeyAction::decode(code);
    return {{QStringLiteral("kind"), int(a.kind)},    {QStringLiteral("modifiers"), a.modifiers},
            {QStringLiteral("key"), a.key},           {QStringLiteral("key2"), a.key2},
            {QStringLiteral("usage"), a.usage},       {QStringLiteral("macroIndex"), a.macroIndex},
            {QStringLiteral("disabled"), a.disabled}, {QStringLiteral("text"), describeAction(code)}};
}

quint32 DeviceController::defaultAction(int layer, int ledIndex) const {
    const KeyInfo* key = keyByLedIndex(ledIndex);
    return key ? defaultWord(layer, *key) : 0;
}

int DeviceController::usageFromQtKey(int qtKey) const {
    bool isModifier = false;
    return hidUsageFromQtKey(qtKey, isModifier);
}
