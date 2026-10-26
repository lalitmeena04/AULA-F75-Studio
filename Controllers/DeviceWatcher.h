#pragma once

#include <QAbstractNativeEventFilter>
#include <QObject>
#include <QTimer>

// Watches Windows device-change broadcasts and tracks whether the keyboard's
// USB command interface and the 2.4 GHz receiver are present. Enumeration is
// debounced because one plug event arrives as several notifications.
class DeviceWatcher final : public QObject, public QAbstractNativeEventFilter {
        Q_OBJECT
        Q_PROPERTY(bool usbPresent READ usbPresent NOTIFY changed)
        Q_PROPERTY(bool receiverPresent READ receiverPresent NOTIFY changed)

    public:
        explicit DeviceWatcher(QObject* parent = nullptr);
        ~DeviceWatcher() override;

        bool usbPresent() const {
            return m_usb;
        }
        bool receiverPresent() const {
            return m_receiver;
        }

        // Re-enumerates now. Returns true when anything changed.
        Q_INVOKABLE bool rescan();

        bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

    signals:
        void changed();
        // Emitted after a rescan with the USB interface newly present / absent.
        void usbArrived();
        void usbRemoved();

    private:
        QTimer m_debounce;
        bool m_usb = false;
        bool m_receiver = false;
};
