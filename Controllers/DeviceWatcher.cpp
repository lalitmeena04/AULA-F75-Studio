#include "DeviceWatcher.h"

#include <windows.h>
#include <dbt.h>

#include <QCoreApplication>

#include "HidTransport.h"
#include "Logging.h"
#include "Protocol.h"

namespace {
constexpr int kDebounceMs = 700;

bool interfacePresent(uint16_t vid, uint16_t pid, uint16_t usagePage, uint16_t usage) {
    QString error;
    for(const auto& match : HidTransport::enumerate(vid, pid, error))
        if(match.usagePage == usagePage && match.usage == usage)
            return true;
    return false;
}
} // namespace

DeviceWatcher::DeviceWatcher(QObject* parent) : QObject(parent) {
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(kDebounceMs);
    connect(&m_debounce, &QTimer::timeout, this, [this] { rescan(); });
    QCoreApplication::instance()->installNativeEventFilter(this);
    // Initial state without signalling arrival; the owner decides what to do at start-up.
    m_usb = interfacePresent(F75::usbVendorId, F75::usbProductId, F75::commandUsagePage, F75::commandUsage);
    m_receiver = interfacePresent(F75::receiverVendorId, F75::receiverProductId, F75::receiverStatusUsagePage,
                                  F75::receiverStatusUsage);
    TRC_INFO("device watcher: usb={} receiver={}", m_usb, m_receiver);
}

DeviceWatcher::~DeviceWatcher() {
    if(auto* app = QCoreApplication::instance())
        app->removeNativeEventFilter(this);
}

bool DeviceWatcher::rescan() {
    const bool usb = interfacePresent(F75::usbVendorId, F75::usbProductId, F75::commandUsagePage, F75::commandUsage);
    const bool receiver = interfacePresent(F75::receiverVendorId, F75::receiverProductId, F75::receiverStatusUsagePage,
                                           F75::receiverStatusUsage);
    const bool usbChanged = usb != m_usb;
    const bool anyChanged = usbChanged || receiver != m_receiver;
    m_usb = usb;
    m_receiver = receiver;
    if(anyChanged) {
        TRC_INFO("device watcher: usb={} receiver={}", m_usb, m_receiver);
        emit changed();
    }
    if(usbChanged) {
        if(usb)
            emit usbArrived();
        else
            emit usbRemoved();
    }
    return anyChanged;
}

bool DeviceWatcher::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) {
    Q_UNUSED(eventType);
    Q_UNUSED(result);
    const auto* msg = static_cast<const MSG*>(message);
    // DBT_DEVNODES_CHANGED reaches every top-level window without registration.
    if(msg->message == WM_DEVICECHANGE && msg->wParam == DBT_DEVNODES_CHANGED)
        m_debounce.start();
    return false;
}
