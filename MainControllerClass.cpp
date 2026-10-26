#include "MainControllerClass.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTimer>

#include "CommandLine.h"
#include "DeviceController.h"
#include "DeviceWatcher.h"
#include "GameModeController.h"
#include "HostEffectsController.h"
#include "LightingPreviewController.h"
#include "Logging.h"
#include "ProfileStore.h"
#include "Version.h"

namespace {
constexpr int kPageCount = 7;
constexpr int kSmokeDelayMs = 1500;
} // namespace

MainControllerClass::MainControllerClass(QGuiApplication& app) : m_app(app) {
    QGuiApplication::setOrganizationName(QStringLiteral("LocalDeviceTools"));
    QGuiApplication::setApplicationName(QStringLiteral("AulaF75Studio"));
    QGuiApplication::setApplicationVersion(QStringLiteral(AULAF75STUDIO_VERSION));

    const QString logFile = F75::initLogging();
    TRC_INFO("==== AULA F75 Studio {} starting ====", AULAF75STUDIO_VERSION);
    TRC_INFO("log file: {}", logFile.toStdString());
    TRC_INFO("os: {} ({}), qt {}", QSysInfo::prettyProductName().toStdString(), QSysInfo::kernelVersion().toStdString(),
             qVersion());

    m_cliExitCode = CommandLine::run(QCoreApplication::arguments());
    if(m_cliExitCode)
        return;

    const QStringList args = QCoreApplication::arguments();
    m_headless = args.contains(QStringLiteral("--smoke")) || args.contains(QStringLiteral("--screenshot-dir"));

    QQuickStyle::setStyle(QStringLiteral("Basic"));
    buildCoreSubsystems();
    registerContextProperties();
    m_engine->loadFromModule("AulaF75Studio", "Main");
}

MainControllerClass::~MainControllerClass() = default;

int MainControllerClass::run() {
    if(m_cliExitCode) {
        F75::flushLogging();
        return *m_cliExitCode;
    }
    if(m_engine->rootObjects().isEmpty()) {
        TRC_ERROR("QML root object failed to load.");
        F75::flushLogging();
        return -1;
    }

    const QStringList args = QCoreApplication::arguments();
    const int shotIndex = args.indexOf(QStringLiteral("--screenshot-dir"));
    const QString shotDir = shotIndex >= 0 && shotIndex + 1 < args.size() ? args.at(shotIndex + 1) : QString();
    if(m_headless)
        armSmokeTest(shotDir);
    else if(m_watcher->usbPresent())
        QTimer::singleShot(0, m_device.get(), &DeviceController::handleUsbArrived); // read-only probe

    const int code = m_app.exec();
    TRC_INFO("==== AULA F75 Studio exiting (code {}) ====", code);
    F75::flushLogging();
    return code;
}

void MainControllerClass::buildCoreSubsystems() {
    m_device = std::make_unique<DeviceController>();
    m_hostEffects = std::make_unique<HostEffectsController>();
    m_preview = std::make_unique<LightingPreviewController>();
    m_gameMode = std::make_unique<GameModeController>();
    m_watcher = std::make_unique<DeviceWatcher>();
    // Hot-plug: the smoke run must stay deterministic, so it does not touch the keyboard.
    if(!m_headless) {
        QObject::connect(m_watcher.get(), &DeviceWatcher::usbArrived, m_device.get(),
                         &DeviceController::handleUsbArrived);
        QObject::connect(m_watcher.get(), &DeviceWatcher::usbRemoved, m_device.get(),
                         &DeviceController::handleUsbRemoved);
    }
    m_storagePath =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/profiles.json");
    m_profiles = std::make_unique<ProfileStore>(m_storagePath);
    TRC_INFO("profiles: {}", m_storagePath.toStdString());

    m_engine = std::make_unique<QQmlApplicationEngine>();
}

void MainControllerClass::registerContextProperties() {
    QQmlContext* ctx = m_engine->rootContext();
    ctx->setContextProperty("device", m_device.get());
    ctx->setContextProperty("hostEffects", m_hostEffects.get());
    ctx->setContextProperty("lightingPreview", m_preview.get());
    ctx->setContextProperty("gameMode", m_gameMode.get());
    ctx->setContextProperty("deviceWatcher", m_watcher.get());
    ctx->setContextProperty("profileStore", m_profiles.get());
    ctx->setContextProperty("appInfo",
                            QVariantMap{{QStringLiteral("appVersion"), QCoreApplication::applicationVersion()},
                                        {QStringLiteral("qtVersion"), QString::fromLatin1(qVersion())},
                                        {QStringLiteral("storagePath"), m_storagePath}});
}

// Renders every page once. Fails the process when a page does not produce an image.
void MainControllerClass::armSmokeTest(const QString& screenshotDir) {
    QTimer::singleShot(kSmokeDelayMs, &m_app, [this, screenshotDir] {
        const auto roots = m_engine->rootObjects();
        auto* window = roots.isEmpty() ? nullptr : qobject_cast<QQuickWindow*>(roots.first());
        if(!window) {
            TRC_ERROR("Smoke test: no window was created.");
            m_app.exit(1);
            return;
        }
        for(int page = 0; page < kPageCount; ++page) {
            window->setProperty("currentPage", page);
            QCoreApplication::processEvents();
            const QImage image = window->grabWindow();
            if(image.isNull()) {
                TRC_ERROR("Smoke test: page {} failed to render.", page);
                m_app.exit(1);
                return;
            }
            if(!screenshotDir.isEmpty() && !image.save(QStringLiteral("%1/page-%2.png").arg(screenshotDir).arg(page))) {
                TRC_ERROR("Could not save screenshot for page {}.", page);
                m_app.exit(1);
                return;
            }
        }
        TRC_INFO("Smoke test rendered all pages.");
        m_app.quit();
    });
}
