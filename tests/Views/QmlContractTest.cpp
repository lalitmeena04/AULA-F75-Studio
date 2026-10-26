#include <gtest/gtest.h>

#include <QFile>
#include <QString>

namespace {

QString loadSource(const char* relativePath) {
    QFile file(QStringLiteral(AULAF75STUDIO_SOURCE_DIR) + QLatin1Char('/') + QString::fromLatin1(relativePath));
    EXPECT_TRUE(file.open(QIODevice::ReadOnly | QIODevice::Text)) << relativePath;
    return QString::fromUtf8(file.readAll());
}

void expectContains(const QString& source, const QString& expected) {
    EXPECT_TRUE(source.contains(expected)) << "Missing QML contract: " << expected.toStdString();
}

void expectNotContains(const QString& source, const QString& unexpected) {
    EXPECT_FALSE(source.contains(unexpected)) << "Stale QML wiring: " << unexpected.toStdString();
}

} // namespace

// Controllers reach QML as context properties. Pages must not re-declare them.
TEST(QmlContractTest, MainUsesContextPropertiesNotRequiredProperties) {
    const QString source = loadSource("Views/Main.qml");

    expectNotContains(source, QStringLiteral("required property var device"));
    expectNotContains(source, QStringLiteral("required property var hostEffects"));
    expectNotContains(source, QStringLiteral("required property var profileStore"));
    expectNotContains(source, QStringLiteral("required property var appInfo"));
    expectContains(source, QStringLiteral("device.startReceiverMonitor()"));
    expectContains(source, QStringLiteral("profileStore.activeIndex"));
    expectContains(source, QStringLiteral("hostEffects.mode = \"off\""));
}

TEST(QmlContractTest, MainStacksEveryPageInOrder) {
    const QString source = loadSource("Views/Main.qml");

    expectContains(source, QStringLiteral("property int currentPage"));
    for(const char* page : {"HomePage {", "LightingPage {", "KeysPage {", "MacrosPage {", "PowerPage {",
                            "ProfilesPage {", "DiagnosticsPage {"})
        expectContains(source, QString::fromLatin1(page));
}

TEST(QmlContractTest, SaveBarGatesApplyOnAVerifiedIdleDevice) {
    const QString source = loadSource("Views/Main.qml");

    expectContains(source, QStringLiteral("visible: work.dirty"));
    expectContains(source, QStringLiteral("enabled: device.verified && !device.busy"));
    expectContains(source, QStringLiteral("device.applyProfile(work.snapshot())"));
}

TEST(QmlContractTest, DiagnosticsPageShowsVersionAndIconSet) {
    const QString source = loadSource("Views/DiagnosticsPage.qml");

    expectContains(source, QStringLiteral("appInfo.appVersion"));
    expectContains(source, QStringLiteral("appInfo.qtVersion"));
    expectContains(source, QStringLiteral("appInfo.storagePath"));
    expectContains(source, QStringLiteral("Icons.activeSet"));
}

TEST(QmlContractTest, DiagnosticsListsInterfacesAndFollowsHotplug) {
    const QString source = loadSource("Views/DiagnosticsPage.qml");

    expectContains(source, QStringLiteral("device.hidInterfaces()"));
    expectContains(source, QStringLiteral("deviceWatcher.rescan()"));
    expectContains(source, QStringLiteral("target: deviceWatcher"));
    expectContains(source, QStringLiteral("deviceWatcher.usbPresent"));
    expectContains(source, QStringLiteral("deviceWatcher.receiverPresent"));
}

TEST(QmlContractTest, LightingPageBindsThePreviewToTheWorkingProfile) {
    const QString source = loadSource("Views/LightingPage.qml");

    for(const char* prop : {"effectIndex", "brightness", "speed", "color", "running"})
        expectContains(source, QStringLiteral("target: lightingPreview; property: \"%1\"").arg(QLatin1String(prop)));
    expectContains(source, QStringLiteral("lightingPreview.frame"));
    // Streaming and per-key canvas take priority over the approximation.
    expectContains(source, QStringLiteral("page.streamingPreview ? page.livePreview : page.custom ? work.perKey"));
}

TEST(QmlContractTest, PowerPageExposesGameModeToggles) {
    const QString source = loadSource("Views/PowerPage.qml");

    expectContains(source, QStringLiteral("gameMode.blockWin = checked"));
    expectContains(source, QStringLiteral("gameMode.blockAltTab = checked"));
    expectContains(source, QStringLiteral("gameMode.blockAltF4 = checked"));
    expectContains(source, QStringLiteral("gameMode.enabled = !gameMode.enabled"));
    expectContains(source, QStringLiteral("gameMode.blockedCount"));
}

TEST(QmlContractTest, ClosingTheWindowReleasesHostSideLocks) {
    const QString source = loadSource("Views/Main.qml");

    expectContains(source, QStringLiteral("gameMode.enabled = false"));
    expectContains(source, QStringLiteral("lightingPreview.running = false"));
}
