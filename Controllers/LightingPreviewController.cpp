#include "LightingPreviewController.h"

#include <algorithm>

#include "Logging.h"
#include "Protocol.h"

LightingPreviewController::LightingPreviewController(QObject* parent) : QObject(parent) {
    m_clock.start();
    m_timer.setTimerType(Qt::CoarseTimer);
    connect(&m_timer, &QTimer::timeout, this, &LightingPreviewController::render);
}

void LightingPreviewController::setEffectIndex(int index) {
    if(m_settings.effectIndex == index)
        return;
    m_settings.effectIndex = index;
    emit settingsChanged();
    render();
    restartTimer();
}

void LightingPreviewController::setBrightness(int brightness) {
    brightness = std::clamp(brightness, 0, 9);
    if(m_settings.brightness == brightness)
        return;
    m_settings.brightness = brightness;
    emit settingsChanged();
    render();
}

void LightingPreviewController::setSpeed(int speed) {
    speed = std::clamp(speed, 0, 4);
    if(m_settings.speed == speed)
        return;
    m_settings.speed = speed;
    emit settingsChanged();
    render();
}

void LightingPreviewController::setColor(int color) {
    color = std::clamp(color, 0, F75::colorCount - 1);
    if(m_settings.color == color)
        return;
    m_settings.color = color;
    emit settingsChanged();
    render();
}

void LightingPreviewController::setRunning(bool running) {
    if(m_running == running)
        return;
    m_running = running;
    TRC_DEBUG("lighting preview {}", running ? "running" : "stopped");
    emit runningChanged();
    if(running)
        render();
    restartTimer();
}

void LightingPreviewController::setFps(int fps) {
    fps = std::clamp(fps, 1, 60);
    if(m_fps == fps)
        return;
    m_fps = fps;
    emit settingsChanged();
    restartTimer();
}

// Static effects need one frame only; the timer stays off for them.
void LightingPreviewController::restartTimer() {
    if(m_running && F75::previewAnimates(m_settings.effectIndex))
        m_timer.start(1000 / m_fps);
    else
        m_timer.stop();
}

void LightingPreviewController::render() {
    if(!m_running)
        return;
    const double seconds = m_clock.elapsed() / 1000.0;
    const QList<QColor> colours = F75::previewFrame(m_settings, seconds);
    QVariantMap frame;
    for(int led = 0; led < colours.size(); ++led) {
        const QColor& c = colours[led];
        if(c.red() == 0 && c.green() == 0 && c.blue() == 0)
            continue;
        frame.insert(QString::number(led), c.name());
    }
    m_frame = frame;
    emit frameChanged();
}
