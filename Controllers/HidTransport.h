#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <cstdint>
#include <memory>
#include <optional>

// One opened HID interface. Blocking; use from a worker thread. The handle is
// owned for the object's lifetime.
class HidTransport {
    public:
        struct Selector {
                uint16_t vendorId;
                uint16_t productId;
                uint16_t usagePage;
                uint16_t usage;
                int featureReportLength; // incl. report ID; -1 = any
                int inputReportLength = -1;
                bool overlapped = false; // required for readInput; HidD_* calls need a synchronous handle
        };
        struct Match {
                QString devicePath;
                uint16_t vendorId;
                uint16_t productId;
                uint16_t usagePage;
                uint16_t usage;
                int featureReportLength;
                int inputReportLength;
                int outputReportLength;
        };

        static QList<Match> enumerate(uint16_t vendorId, uint16_t productId, QString& error);
        static std::unique_ptr<HidTransport> open(const Selector& selector, QString& error);

        ~HidTransport();
        HidTransport(const HidTransport&) = delete;
        HidTransport& operator=(const HidTransport&) = delete;

        const Match& match() const {
            return m_match;
        }
        // Retried 3x with backoff.
        bool setFeature(const QByteArray& report, QString& error);
        std::optional<QByteArray> getFeature(uint8_t reportId, int length, QString& error);
        // Waits up to timeoutMs for one input report; nullopt on timeout/error.
        std::optional<QByteArray> readInput(int timeoutMs, QString& error);
        void cancelRead();

    private:
        HidTransport(void* handle, Match match);
        void* m_handle;
        void* m_readEvent = nullptr;
        bool m_overlapped = false;
        Match m_match;
};
