#include "Device.h"
#include "HidTransport.h"
#include "HostEffects.h"
#include "ProfileStore.h"
#include "Protocol.h"

#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTimer>
#include <cstdio>

namespace {
void emitJson(const QJsonObject &object)
{
    const QByteArray json = QJsonDocument(object).toJson();
    std::fwrite(json.constData(), 1, static_cast<size_t>(json.size()), stdout);
}
}

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("LocalDeviceTools"));
    QCoreApplication::setApplicationName(QStringLiteral("AulaF75Studio"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    const QStringList args = app.arguments();

    if (args.contains(QStringLiteral("--hid-list"))) {
        QJsonArray rows;
        QString error;
        for (const auto &[vid, pid] : {std::pair{F75::receiverVendorId, F75::receiverProductId},
                                       std::pair{F75::usbVendorId, F75::usbProductId}}) {
            for (const auto &m : HidTransport::enumerate(vid, pid, error))
                rows.append(QJsonObject{
                    {QStringLiteral("vid"), QStringLiteral("%1").arg(m.vendorId, 4, 16, QLatin1Char('0'))},
                    {QStringLiteral("pid"), QStringLiteral("%1").arg(m.productId, 4, 16, QLatin1Char('0'))},
                    {QStringLiteral("usagePage"), QStringLiteral("%1").arg(m.usagePage, 4, 16, QLatin1Char('0'))},
                    {QStringLiteral("usage"), QStringLiteral("%1").arg(m.usage, 4, 16, QLatin1Char('0'))},
                    {QStringLiteral("feature"), m.featureReportLength}, {QStringLiteral("input"), m.inputReportLength},
                    {QStringLiteral("output"), m.outputReportLength}, {QStringLiteral("path"), m.devicePath}});
        }
        emitJson({{QStringLiteral("interfaces"), rows}, {QStringLiteral("error"), error}});
        return 0;
    }
    if (args.contains(QStringLiteral("--probe"))) {
        const auto outcome = Device::runProbe();
        emitJson({{QStringLiteral("state"), outcome.state}, {QStringLiteral("link"), outcome.link},
                  {QStringLiteral("message"), outcome.message},
                  {QStringLiteral("current"), QJsonObject::fromVariantMap(outcome.current)},
                  {QStringLiteral("log"), QJsonArray::fromStringList(outcome.log)}, {QStringLiteral("wrote"), false}});
        return outcome.state == QStringLiteral("verified") ? 0 : 2;
    }
    if (args.contains(QStringLiteral("--dump"))) {
        // Read-only: key layers 0-1 and the macro buffer, decoded with the recovered encoding.
        const auto dump = Device::runDump();
        emitJson(dump);
        return dump.value(QStringLiteral("state")).toString() == QStringLiteral("verified") ? 0 : 2;
    }
    if (args.contains(QStringLiteral("--stream-test"))) {
        // Transient: a 3-second rainbow sweep through the real-time frame command, then a re-probe.
        const auto result = Device::runStreamTest();
        emitJson(result);
        return result.value(QStringLiteral("state")).toString() == QStringLiteral("verified") ? 0 : 2;
    }
    if (args.contains(QStringLiteral("--matrix-test"))) {
        // Writes layer 1 twice (one key changed, then the original); the keyboard ends unchanged.
        const auto result = Device::runMatrixTest();
        emitJson(result);
        return result.value(QStringLiteral("state")).toString() == QStringLiteral("verified") ? 0 : 2;
    }

    QQuickStyle::setStyle(QStringLiteral("Basic"));
    Device device;
    HostEffects hostEffects;
    const QString storagePath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
                                QStringLiteral("/profiles.json");
    ProfileStore profiles(storagePath);

    QQmlApplicationEngine engine;
    engine.setInitialProperties({
        {QStringLiteral("device"), QVariant::fromValue(&device)},
        {QStringLiteral("hostEffects"), QVariant::fromValue(&hostEffects)},
        {QStringLiteral("profileStore"), QVariant::fromValue(&profiles)},
        {QStringLiteral("appInfo"), QVariantMap{
            {QStringLiteral("appVersion"), QCoreApplication::applicationVersion()},
            {QStringLiteral("qtVersion"), QString::fromLatin1(qVersion())},
            {QStringLiteral("storagePath"), storagePath}}},
    });
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule(QStringLiteral("AulaF75Studio"), QStringLiteral("Main"));

    const int shotIndex = args.indexOf(QStringLiteral("--screenshot-dir"));
    const QString shotDir = shotIndex >= 0 && shotIndex + 1 < args.size() ? args.at(shotIndex + 1) : QString();
    if (args.contains(QStringLiteral("--smoke")) || !shotDir.isEmpty()) {
        QTimer::singleShot(1500, &app, [&] {
            const auto roots = engine.rootObjects();
            auto *window = roots.isEmpty() ? nullptr : qobject_cast<QQuickWindow *>(roots.first());
            if (!window) {
                qCritical("Smoke test: no window was created.");
                app.exit(1);
                return;
            }
            for (int page = 0; page < 7; ++page) {
                window->setProperty("currentPage", page);
                QCoreApplication::processEvents();
                const QImage image = window->grabWindow();
                if (image.isNull()) {
                    qCritical("Smoke test: page %d failed to render.", page);
                    app.exit(1);
                    return;
                }
                if (!shotDir.isEmpty() && !image.save(QStringLiteral("%1/page-%2.png").arg(shotDir).arg(page))) {
                    qCritical("Could not save screenshot for page %d.", page);
                    app.exit(1);
                    return;
                }
            }
            qInfo("Smoke test rendered all pages.");
            app.quit();
        });
    }
    return app.exec();
}
