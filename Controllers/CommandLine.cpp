#include "CommandLine.h"

#include <cstdio>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "DeviceController.h"
#include "HidTransport.h"
#include "Logging.h"
#include "Protocol.h"

namespace {

void emitJson(const QJsonObject& object) {
    const QByteArray json = QJsonDocument(object).toJson();
    std::fwrite(json.constData(), 1, static_cast<size_t>(json.size()), stdout);
}

int exitCodeFor(const QJsonObject& result) {
    return result.value(QStringLiteral("state")).toString() == QStringLiteral("verified") ? 0 : 2;
}

QJsonObject hidList() {
    QJsonArray rows;
    QString error;
    for(const auto& [vid, pid] :
        {std::pair{F75::receiverVendorId, F75::receiverProductId}, std::pair{F75::usbVendorId, F75::usbProductId}}) {
        for(const auto& m : HidTransport::enumerate(vid, pid, error)) {
            rows.append(QJsonObject{
                {QStringLiteral("vid"), QStringLiteral("%1").arg(m.vendorId, 4, 16, QLatin1Char('0'))},
                {QStringLiteral("pid"), QStringLiteral("%1").arg(m.productId, 4, 16, QLatin1Char('0'))},
                {QStringLiteral("usagePage"), QStringLiteral("%1").arg(m.usagePage, 4, 16, QLatin1Char('0'))},
                {QStringLiteral("usage"), QStringLiteral("%1").arg(m.usage, 4, 16, QLatin1Char('0'))},
                {QStringLiteral("feature"), m.featureReportLength},
                {QStringLiteral("input"), m.inputReportLength},
                {QStringLiteral("output"), m.outputReportLength},
                {QStringLiteral("path"), m.devicePath}});
        }
    }
    return {{QStringLiteral("interfaces"), rows}, {QStringLiteral("error"), error}};
}

} // namespace

std::optional<int> CommandLine::run(const QStringList& args) {
    if(args.contains(QStringLiteral("--hid-list"))) {
        TRC_INFO("cli: --hid-list");
        emitJson(hidList());
        return 0;
    }
    if(args.contains(QStringLiteral("--probe"))) {
        TRC_INFO("cli: --probe");
        const auto outcome = DeviceController::runProbe();
        emitJson({{QStringLiteral("state"), outcome.state},
                  {QStringLiteral("link"), outcome.link},
                  {QStringLiteral("message"), outcome.message},
                  {QStringLiteral("current"), QJsonObject::fromVariantMap(outcome.current)},
                  {QStringLiteral("log"), QJsonArray::fromStringList(outcome.log)},
                  {QStringLiteral("wrote"), false}});
        return outcome.state == QStringLiteral("verified") ? 0 : 2;
    }
    if(args.contains(QStringLiteral("--dump"))) {
        // Read-only: key layers 0-1 and the macro buffer, decoded.
        TRC_INFO("cli: --dump");
        const auto dump = DeviceController::runDump();
        emitJson(dump);
        return exitCodeFor(dump);
    }
    if(args.contains(QStringLiteral("--stream-test"))) {
        // Transient: a 3-second rainbow sweep through the real-time frame command, then a re-probe.
        TRC_INFO("cli: --stream-test");
        const auto result = DeviceController::runStreamTest();
        emitJson(result);
        return exitCodeFor(result);
    }
    if(args.contains(QStringLiteral("--matrix-test"))) {
        // Writes layer 1 twice (one key changed, then the original); the keyboard ends unchanged.
        TRC_INFO("cli: --matrix-test");
        const auto result = DeviceController::runMatrixTest();
        emitJson(result);
        return exitCodeFor(result);
    }
    return std::nullopt;
}
