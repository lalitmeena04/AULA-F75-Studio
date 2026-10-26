#include "HidTransport.h"

#include <QThread>
#include <qt_windows.h>
#include <hidsdi.h>
#include <setupapi.h>
#include <vector>

namespace {
QString lastError(const char *what)
{
    const DWORD code = GetLastError();
    wchar_t *message = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, code, 0, reinterpret_cast<wchar_t *>(&message), 0, nullptr);
    const QString text = message ? QString::fromWCharArray(message).trimmed() : QStringLiteral("unknown error");
    if (message)
        LocalFree(message);
    return QStringLiteral("%1: %2 (%3)").arg(QLatin1String(what), text).arg(code);
}

struct Closer {
    void operator()(void *h) const { if (h && h != INVALID_HANDLE_VALUE) CloseHandle(h); }
};
using Handle = std::unique_ptr<void, Closer>;

struct InfoList {
    HDEVINFO handle = INVALID_HANDLE_VALUE;
    ~InfoList() { if (handle != INVALID_HANDLE_VALUE) SetupDiDestroyDeviceInfoList(handle); }
};

template <typename Visit>
void forEachInterface(QString &error, Visit visit)
{
    GUID hidGuid;
    HidD_GetHidGuid(&hidGuid);
    InfoList set{SetupDiGetClassDevsW(&hidGuid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE)};
    if (set.handle == INVALID_HANDLE_VALUE) {
        error = lastError("SetupDiGetClassDevs");
        return;
    }
    for (DWORD index = 0;; ++index) {
        SP_DEVICE_INTERFACE_DATA interfaceData{};
        interfaceData.cbSize = sizeof(interfaceData);
        if (!SetupDiEnumDeviceInterfaces(set.handle, nullptr, &hidGuid, index, &interfaceData)) {
            if (GetLastError() != ERROR_NO_MORE_ITEMS)
                error = lastError("SetupDiEnumDeviceInterfaces");
            return;
        }
        DWORD required = 0;
        SetupDiGetDeviceInterfaceDetailW(set.handle, &interfaceData, nullptr, 0, &required, nullptr);
        if (required == 0)
            continue;
        std::vector<std::byte> buffer(required);
        auto *detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W *>(buffer.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        if (!SetupDiGetDeviceInterfaceDetailW(set.handle, &interfaceData, detail, required, nullptr, nullptr))
            continue;
        if (visit(QString::fromWCharArray(detail->DevicePath)))
            return;
    }
}

std::optional<HidTransport::Match> inspect(const QString &path, Handle &outHandle, bool overlapped)
{
    const DWORD flags = overlapped ? FILE_FLAG_OVERLAPPED : 0;
    Handle handle(CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), GENERIC_READ | GENERIC_WRITE,
                              FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, flags, nullptr));
    if (!handle || handle.get() == INVALID_HANDLE_VALUE)
        handle.reset(CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), 0,
                                 FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, flags, nullptr));
    if (!handle || handle.get() == INVALID_HANDLE_VALUE)
        return std::nullopt;
    HIDD_ATTRIBUTES attributes{};
    attributes.Size = sizeof(attributes);
    if (!HidD_GetAttributes(handle.get(), &attributes))
        return std::nullopt;
    PHIDP_PREPARSED_DATA preparsed = nullptr;
    if (!HidD_GetPreparsedData(handle.get(), &preparsed))
        return std::nullopt;
    HIDP_CAPS caps{};
    const NTSTATUS status = HidP_GetCaps(preparsed, &caps);
    HidD_FreePreparsedData(preparsed);
    if (status != HIDP_STATUS_SUCCESS)
        return std::nullopt;
    outHandle = std::move(handle);
    return HidTransport::Match{path, attributes.VendorID, attributes.ProductID, caps.UsagePage, caps.Usage,
                               caps.FeatureReportByteLength, caps.InputReportByteLength, caps.OutputReportByteLength};
}
}

QList<HidTransport::Match> HidTransport::enumerate(uint16_t vendorId, uint16_t productId, QString &error)
{
    QList<Match> matches;
    forEachInterface(error, [&](const QString &path) {
        Handle handle;
        if (auto match = inspect(path, handle, false); match && match->vendorId == vendorId && match->productId == productId)
            matches.append(*match);
        return false;
    });
    return matches;
}

std::unique_ptr<HidTransport> HidTransport::open(const Selector &selector, QString &error)
{
    std::unique_ptr<HidTransport> result;
    forEachInterface(error, [&](const QString &path) {
        Handle handle;
        auto match = inspect(path, handle, selector.overlapped);
        if (!match || match->vendorId != selector.vendorId || match->productId != selector.productId ||
            match->usagePage != selector.usagePage || match->usage != selector.usage ||
            (selector.featureReportLength >= 0 && match->featureReportLength != selector.featureReportLength) ||
            (selector.inputReportLength >= 0 && match->inputReportLength != selector.inputReportLength))
            return false;
        result.reset(new HidTransport(handle.release(), *match));
        result->m_overlapped = selector.overlapped;
        return true;
    });
    if (!result && error.isEmpty())
        error = QStringLiteral("No HID interface with VID %1 PID %2, usage %3/%4 is present.")
                    .arg(selector.vendorId, 4, 16, QLatin1Char('0')).arg(selector.productId, 4, 16, QLatin1Char('0'))
                    .arg(selector.usagePage, 4, 16, QLatin1Char('0')).arg(selector.usage, 4, 16, QLatin1Char('0'));
    return result;
}

HidTransport::HidTransport(void *handle, Match match)
    : m_handle(handle), m_readEvent(CreateEventW(nullptr, TRUE, FALSE, nullptr)), m_match(std::move(match)) {}

HidTransport::~HidTransport()
{
    cancelRead();
    Closer{}(m_handle);
    Closer{}(m_readEvent);
}

bool HidTransport::setFeature(const QByteArray &report, QString &error)
{
    if (report.size() != m_match.featureReportLength) {
        error = QStringLiteral("Report length %1 does not match interface length %2.")
                    .arg(report.size()).arg(m_match.featureReportLength);
        return false;
    }
    for (int attempt = 0; attempt < 3; ++attempt) {
        if (HidD_SetFeature(m_handle, const_cast<char *>(report.constData()), static_cast<ULONG>(report.size())))
            return true;
        error = lastError("HidD_SetFeature");
        QThread::msleep(70);
    }
    return false;
}

std::optional<QByteArray> HidTransport::getFeature(uint8_t reportId, int length, QString &error)
{
    if (length != m_match.featureReportLength) {
        error = QStringLiteral("Report length %1 does not match interface length %2.")
                    .arg(length).arg(m_match.featureReportLength);
        return std::nullopt;
    }
    QByteArray report(length, '\0');
    report[0] = static_cast<char>(reportId);
    for (int attempt = 0; attempt < 3; ++attempt) {
        if (HidD_GetFeature(m_handle, report.data(), static_cast<ULONG>(report.size())))
            return report;
        error = lastError("HidD_GetFeature");
        QThread::msleep(70);
    }
    return std::nullopt;
}

std::optional<QByteArray> HidTransport::readInput(int timeoutMs, QString &error)
{
    if (m_match.inputReportLength <= 0) {
        error = QStringLiteral("Interface has no input reports.");
        return std::nullopt;
    }
    if (!m_overlapped) {
        error = QStringLiteral("Interface was opened without overlapped I/O.");
        return std::nullopt;
    }
    QByteArray report(m_match.inputReportLength, '\0');
    OVERLAPPED overlapped{};
    overlapped.hEvent = m_readEvent;
    ResetEvent(m_readEvent);
    DWORD read = 0;
    if (!ReadFile(m_handle, report.data(), static_cast<DWORD>(report.size()), &read, &overlapped)) {
        if (GetLastError() != ERROR_IO_PENDING) {
            error = lastError("ReadFile");
            return std::nullopt;
        }
        if (WaitForSingleObject(m_readEvent, static_cast<DWORD>(timeoutMs)) != WAIT_OBJECT_0) {
            CancelIoEx(m_handle, &overlapped);
            GetOverlappedResult(m_handle, &overlapped, &read, TRUE);
            return std::nullopt;
        }
        if (!GetOverlappedResult(m_handle, &overlapped, &read, FALSE)) {
            error = lastError("GetOverlappedResult");
            return std::nullopt;
        }
    }
    report.truncate(static_cast<int>(read));
    return report;
}

void HidTransport::cancelRead()
{
    if (m_handle && m_handle != INVALID_HANDLE_VALUE)
        CancelIoEx(m_handle, nullptr);
}
