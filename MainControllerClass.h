#pragma once

#include <memory>
#include <optional>

#include <QString>

class QGuiApplication;
class QQmlApplicationEngine;

class DeviceController;
class DeviceWatcher;
class GameModeController;
class HostEffectsController;
class LightingPreviewController;
class ProfileStore;

class MainControllerClass {
    public:
        explicit MainControllerClass(QGuiApplication& app);
        ~MainControllerClass();

        int run();

    private:
        void buildCoreSubsystems();
        void registerContextProperties();
        void armSmokeTest(const QString& screenshotDir);

        QGuiApplication& m_app;
        std::optional<int> m_cliExitCode;
        QString m_storagePath;
        bool m_headless = false;

        // Construction order also sets teardown order.
        std::unique_ptr<DeviceController> m_device;
        std::unique_ptr<HostEffectsController> m_hostEffects;
        std::unique_ptr<LightingPreviewController> m_preview;
        std::unique_ptr<GameModeController> m_gameMode;
        std::unique_ptr<DeviceWatcher> m_watcher;
        std::unique_ptr<ProfileStore> m_profiles;
        std::unique_ptr<QQmlApplicationEngine> m_engine;
};
