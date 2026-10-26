#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <QVariantMap>

#include "LightingPreview.h"

// Animates the host-side approximation of a built-in effect for the keyboard
// canvas. QML sets the effect parameters and `running`; `frame` maps LED
// index -> "#rrggbb" and refreshes at `fps` while running.
class LightingPreviewController final : public QObject {
        Q_OBJECT
        Q_PROPERTY(int effectIndex READ effectIndex WRITE setEffectIndex NOTIFY settingsChanged)
        Q_PROPERTY(int brightness READ brightness WRITE setBrightness NOTIFY settingsChanged)
        Q_PROPERTY(int speed READ speed WRITE setSpeed NOTIFY settingsChanged)
        Q_PROPERTY(int color READ color WRITE setColor NOTIFY settingsChanged)
        Q_PROPERTY(bool running READ running WRITE setRunning NOTIFY runningChanged)
        Q_PROPERTY(int fps READ fps WRITE setFps NOTIFY settingsChanged)
        Q_PROPERTY(QVariantMap frame READ frame NOTIFY frameChanged)

    public:
        explicit LightingPreviewController(QObject* parent = nullptr);

        int effectIndex() const {
            return m_settings.effectIndex;
        }
        void setEffectIndex(int index);
        int brightness() const {
            return m_settings.brightness;
        }
        void setBrightness(int brightness);
        int speed() const {
            return m_settings.speed;
        }
        void setSpeed(int speed);
        int color() const {
            return m_settings.color;
        }
        void setColor(int color);
        bool running() const {
            return m_running;
        }
        void setRunning(bool running);
        int fps() const {
            return m_fps;
        }
        void setFps(int fps);
        QVariantMap frame() const {
            return m_frame;
        }

    signals:
        void settingsChanged();
        void runningChanged();
        void frameChanged();

    private:
        void render();
        void restartTimer();

        F75::PreviewSettings m_settings;
        bool m_running = false;
        int m_fps = 15;
        QTimer m_timer;
        QElapsedTimer m_clock;
        QVariantMap m_frame;
};
