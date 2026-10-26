#pragma once

#include "HostEffects.h"
#include "Protocol.h"

#include <QColor>
#include <QFutureWatcher>
#include <QJsonObject>
#include <QMutex>
#include <QObject>
#include <QQmlEngine>
#include <QVariantList>
#include <QVariantMap>
#include <QWaitCondition>
#include <atomic>
#include <functional>
#include <thread>

// Owns every exchange with the keyboard. Operations run serialized on a worker
// thread. Writes are refused until a probe over the USB command interface has
// verified the identity and a signature-valid config block; each write is a
// read-modify-write of the device's own data followed by a readback.
class Device : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by main")
    Q_PROPERTY(QString state READ state NOTIFY changed)         // idle, busy, verified, receiver, absent, mismatch, error
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(QString link READ link NOTIFY changed)           // usb, receiver, none
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool verified READ verified NOTIFY changed)
    Q_PROPERTY(bool streaming READ streaming NOTIFY changed)
    Q_PROPERTY(QVariantMap current READ current NOTIFY changed)
    Q_PROPERTY(QVariantMap keymap READ keymap NOTIFY changed)   // layer -> {led -> code}
    Q_PROPERTY(QVariantList macros READ macros NOTIFY changed)
    Q_PROPERTY(QVariantMap receiverStatus READ receiverStatus NOTIFY receiverChanged)
    Q_PROPERTY(QVariantList log READ log NOTIFY changed)
    Q_PROPERTY(QVariantList effects READ effects CONSTANT)
    Q_PROPERTY(QVariantList colors READ colors CONSTANT)
    Q_PROPERTY(QVariantList sleepPresets READ sleepPresets CONSTANT)
    Q_PROPERTY(QVariantList keys READ keys CONSTANT)
    Q_PROPERTY(QVariantList hidUsages READ hidUsages CONSTANT)
    Q_PROPERTY(QVariantList consumerUsages READ consumerUsages CONSTANT)
    Q_PROPERTY(QVariantList deviceFunctions READ deviceFunctions CONSTANT)   // factory Fn-layer words
public:
    explicit Device(QObject *parent = nullptr);
    ~Device() override;

    QString state() const { return m_state; }
    QString message() const { return m_message; }
    QString link() const { return m_link; }
    bool busy() const { return m_watcher.isRunning() || m_streaming; }
    bool verified() const { return m_state == QStringLiteral("verified") && m_link == QStringLiteral("usb"); }
    bool streaming() const { return m_streaming; }
    QVariantMap current() const { return m_current; }
    QVariantMap keymap() const { return m_keymap; }
    QVariantList macros() const { return m_macros; }
    QVariantMap receiverStatus() const;
    QVariantList log() const { return m_log; }
    QVariantList effects() const;
    QVariantList colors() const;
    QVariantList sleepPresets() const;
    QVariantList keys() const;
    QVariantList hidUsages() const;
    QVariantList consumerUsages() const;
    QVariantList deviceFunctions() const;

    Q_INVOKABLE void probe();
    Q_INVOKABLE void readKeymap(int layer);
    Q_INVOKABLE void readMacros();
    Q_INVOKABLE void applyEffect(int effectIndex, int brightness, int speed, int color);
    Q_INVOKABLE void applySleep(int seconds);
    Q_INVOKABLE void applyPerKey(const QVariantMap &ledToColor);
    Q_INVOKABLE void applyKeymap(int layer, const QVariantMap &ledToCode);
    Q_INVOKABLE void applyMacros(const QVariantList &macros);
    Q_INVOKABLE void applyProfile(const QVariantMap &profile);
    Q_INVOKABLE void factoryReset();

    Q_INVOKABLE void startStream(HostEffects *source);
    Q_INVOKABLE void stopStream();
    Q_INVOKABLE void startReceiverMonitor();
    Q_INVOKABLE void stopReceiverMonitor();

    // Key-action helpers for QML (encoded as the device's 32-bit matrix word).
    Q_INVOKABLE quint32 encodeKeyboard(int modifiers, int usage, int usage2) const;
    Q_INVOKABLE quint32 encodeConsumer(int usage) const;
    Q_INVOKABLE quint32 encodeMacro(int macroIndex, int mode, int repeat) const;
    Q_INVOKABLE quint32 encodeMouse(int button) const;
    Q_INVOKABLE quint32 encodeProfile(int profileIndex) const;
    Q_INVOKABLE QString describeAction(quint32 code) const;
    Q_INVOKABLE QVariantMap decodeAction(quint32 code) const;
    Q_INVOKABLE quint32 defaultAction(int layer, int ledIndex) const;
    Q_INVOKABLE int usageFromQtKey(int qtKey) const;

    struct Outcome {
        QString state;
        QString message;
        QString link;
        QVariantMap current;
        QVariantMap keymapLayer;    // {"layer": n, "entries": {led: code}}
        QVariantList macros;
        QStringList log;
        bool wrote = false;
    };
    static Outcome runProbe();
    // Read-only CLI dump of key layers 0-1 and the macro buffer.
    static QJsonObject runDump();
    // Sends ~3 s of real-time frames (command 0x08) at 10 fps, then re-probes identity.
    static QJsonObject runStreamTest();
    // Reversible write test: changes Fn+O on layer 1, reads back, restores the original layer, reads back.
    static QJsonObject runMatrixTest();

signals:
    void changed();
    void receiverChanged();

private:
    void start(std::function<Outcome()> job);
    void finish();
    void onFrame(QList<QColor> frame);
    void streamLoop();
    void monitorLoop();

    QFutureWatcher<Outcome> m_watcher;
    QString m_state = QStringLiteral("idle");
    QString m_message = QStringLiteral("Not connected. Press Connect to probe the keyboard.");
    QString m_link = QStringLiteral("none");
    QVariantMap m_current;
    QVariantMap m_keymap;
    QVariantList m_macros;
    QVariantList m_log;

    std::atomic<bool> m_streaming{false};
    std::atomic<bool> m_streamStop{false};
    std::thread m_streamThread;
    QMutex m_frameMutex;
    QWaitCondition m_frameReady;
    QList<QColor> m_pendingFrame;
    bool m_hasFrame = false;
    HostEffects *m_source = nullptr;

    std::atomic<bool> m_monitorStop{false};
    std::thread m_monitorThread;
    mutable QMutex m_receiverMutex;
    QVariantMap m_receiver;
};
