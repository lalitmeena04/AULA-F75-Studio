#include "LightingPreview.h"

#include <algorithm>
#include <cmath>

#include "KeyLayout.h"
#include "Protocol.h"

namespace F75 {

namespace {

constexpr double kPi = 3.14159265358979323846;

struct Rgb {
        double r = 0, g = 0, b = 0;
        Rgb scaled(double factor) const {
            return {std::clamp(r * factor, 0.0, 1.0), std::clamp(g * factor, 0.0, 1.0),
                    std::clamp(b * factor, 0.0, 1.0)};
        }
};

double fract(double value) {
    return value - std::floor(value);
}

// Repeatable pseudo-random number in [0, 1) for the given inputs.
double hash(double a, double b, double c) {
    return fract(std::sin(a * 12.9898 + b * 78.233 + c * 37.719) * 43758.5453);
}

Rgb hsv(double hue, double saturation, double value) {
    const double sector = fract(hue) * 6.0;
    const int index = static_cast<int>(std::floor(sector)) % 6;
    const double f = sector - std::floor(sector);
    const double p = value * (1 - saturation);
    const double q = value * (1 - saturation * f);
    const double t = value * (1 - saturation * (1 - f));
    switch(index) {
    case 0:
        return {value, t, p};
    case 1:
        return {q, value, p};
    case 2:
        return {p, value, t};
    case 3:
        return {p, q, value};
    case 4:
        return {t, p, value};
    default:
        return {value, p, q};
    }
}

// Full-intensity colour: rainbow hue when colourful, else the preset colour.
Rgb base(const PreviewSettings& settings, bool forceColourful, double hue) {
    if(forceColourful || settings.color >= colorCount - 1 || settings.color < 0)
        return hsv(hue, 1, 1);
    const QColor preset = presetColor(settings.color);
    return {preset.redF(), preset.greenF(), preset.blueF()};
}

double breath(double phase) {
    return 0.5 - 0.5 * std::cos(phase * 2 * kPi * 0.5);
}

// 1 on the crest of a repeating wave, falling to 0 between crests.
double ring(double value) {
    return std::pow(0.5 + 0.5 * std::cos(fract(value) * 2 * kPi), 3);
}

// The board is about 2.3x wider than tall; squash y so rings look round.
double centreDistance(double x, double y) {
    const double dx = x - 0.5;
    const double dy = (y - 0.5) * 0.44;
    return std::sqrt(dx * dx + dy * dy);
}

double angle(double x, double y) {
    return std::atan2((y - 0.5) * 0.44, x - 0.5) / (2 * kPi);
}

// A key "pressed" at a random moment every cycle, glowing then fading.
double pressFlash(double x, double y, double seconds, double rate, double threshold) {
    const double cycle = seconds * rate * 1.5 + hash(x, y, 7) * 8;
    const double age = fract(cycle);
    const bool pressedThisCycle = hash(x, y, std::floor(cycle)) > threshold;
    return pressedThisCycle ? std::pow(1 - age, 2) : 0.0;
}

double sparkle(double x, double y, double seconds, double rate) {
    const double cycle = seconds * rate * 3 + hash(x, y, 3) * 5;
    const bool visible = hash(x, y, std::floor(cycle)) > 0.55;
    return visible ? 1 - fract(cycle) * 0.7 : 0.08;
}

double fallingStreak(double x, double y, double phase, double seed) {
    return std::pow(fract(y - phase * 1.5 + hash(std::floor(x * 16), seed, 0)), 4);
}

Rgb ripples(const PreviewSettings& settings, bool colourful, double x, double y, double phase) {
    double intensity = 0;
    double hue = 0;
    for(int drop = 0; drop < 3; ++drop) {
        const double cycle = phase * 0.6 + drop / 3.0;
        const double index = std::floor(cycle);
        const double centreX = hash(index, drop, 11);
        const double centreY = hash(index, drop, 13);
        const double dx = x - centreX;
        const double dy = (y - centreY) * 0.44;
        const double distance = std::sqrt(dx * dx + dy * dy);
        const double radius = fract(cycle) * 0.6;
        const double strength = std::max(0.0, 1 - std::abs(distance - radius) * 14) * (1 - fract(cycle));
        if(strength > intensity) {
            intensity = strength;
            hue = hash(index, drop, 17);
        }
    }
    return base(settings, colourful, hue).scaled(intensity);
}

// Serpentine path over six rows: left to right, then right to left.
double snakePosition(double x, double y) {
    const int rows = 6;
    const int row = std::clamp(static_cast<int>(y * rows), 0, rows - 1);
    const double along = (row % 2 == 0) ? x : 1 - x;
    return (row + along) / rows;
}

} // namespace

QColor previewColor(const PreviewSettings& settings, double x, double y, double seconds) {
    const Effect* effect = effectByIndex(settings.effectIndex);
    if(!effect || settings.effectIndex == offEffectIndex || settings.effectIndex == customEffectIndex)
        return QColor(0, 0, 0);

    const bool colourful = !effect->hasColor;
    const double rate = 0.15 + (std::clamp(settings.speed, 0, 4) + 1) * 0.15;
    const double brightness = (std::clamp(settings.brightness, 0, 9) + 1) / 10.0;
    const double phase = seconds * rate;

    Rgb colour;
    switch(settings.effectIndex) {
    case 0: // Static
        colour = base(settings, colourful, x * 0.85);
        break;
    case 1: // Breathing: whole board, new hue each breath
        colour = base(settings, colourful, std::floor(phase * 0.5) * 0.17).scaled(breath(phase));
        break;
    case 2: // Rainbow: the whole board cycles through hues
        colour = base(settings, true, phase * 0.25);
        break;
    case 3: // Flash Away: keys light as if pressed, then fade
        colour = base(settings, colourful, x).scaled(pressFlash(x, y, seconds, rate, 0.82));
        break;
    case 4: // Raindrops: streaks running down each column
        colour = base(settings, colourful, x).scaled(fallingStreak(x, y, phase, 0));
        break;
    case 5: // Rainbow Wheel: hues sweep around the centre
        colour = base(settings, colourful, angle(x, y) + phase * 0.5);
        break;
    case 6: // Ripples
        colour = ripples(settings, colourful, x, y, phase);
        break;
    case 7: // Stars Twinkle
        colour =
            base(settings, colourful, hash(x, y, std::floor(seconds * rate * 6))).scaled(sparkle(x, y, seconds, rate));
        break;
    case 8: // Shadow Disappear: lit board, pressed keys go dark briefly
        colour = base(settings, colourful, x).scaled(1 - pressFlash(x, y, seconds, rate, 0.82));
        break;
    case 9: { // Retro Snake: a bright head running along a serpentine path
        const double head = fract(phase * 0.5);
        const double behind = fract(head - snakePosition(x, y));
        colour = base(settings, colourful, head).scaled(std::max(0.0, 1 - behind * 10));
        break;
    }
    case 10: // Neon Stream: rainbow wave rolling left to right
        colour = base(settings, colourful, x - phase).scaled(0.6 + 0.4 * ring(x * 2 - phase * 2));
        break;
    case 11: // Reaction: quick flash on pressed keys only
        colour = base(settings, colourful, hash(x, y, 5)).scaled(pressFlash(x, y, seconds, rate * 1.4, 0.9));
        break;
    case 12: { // Sine Wave: a band moving along x, bent by y
        const double band = x * 1.5 - phase * 1.5 + std::sin(y * kPi) * 0.2;
        colour = base(settings, colourful, x - phase).scaled(ring(band));
        break;
    }
    case 13: { // Retinue Scanning: a bar sliding back and forth
        const double position = 0.5 - 0.5 * std::cos(phase * kPi);
        colour = base(settings, colourful, position).scaled(std::max(0.0, 1 - std::abs(x - position) * 6));
        break;
    }
    case 14: { // Rotating Windmill: three blades turning about the centre
        const double a = angle(x, y) + phase * 0.5;
        colour = base(settings, true, a).scaled(ring(a * 3));
        break;
    }
    case 15: // Colorful Waterfall: rainbow streaks falling
        colour = base(settings, true, x + phase * 0.2).scaled(fallingStreak(x, y, phase, 1));
        break;
    case 16: { // Blossoming: rings growing out from the centre
        const double distance = centreDistance(x, y);
        colour = base(settings, true, distance - phase).scaled(ring(distance * 2 - phase * 2));
        break;
    }
    case 17: { // Rotating Storm: a spiral turning about the centre
        const double a = angle(x, y);
        const double distance = centreDistance(x, y);
        colour = base(settings, colourful, a + distance - phase).scaled(ring(a * 2 + distance * 4 - phase * 2));
        break;
    }
    default: // Unknown effect: slow rainbow so something sensible shows
        colour = base(settings, true, x - seconds * 0.1);
        break;
    }

    const Rgb out = colour.scaled(brightness);
    return QColor::fromRgbF(out.r, out.g, out.b);
}

QList<QColor> previewFrame(const PreviewSettings& settings, double seconds) {
    QList<QColor> frame(matrixLength, QColor(0, 0, 0));
    for(const KeyInfo& key : keyLayout()) {
        if(key.ledIndex < 0 || key.ledIndex >= matrixLength)
            continue;
        const double x = key.rect.center().x() / double(layoutWidth);
        const double y = key.rect.center().y() / double(layoutHeight);
        frame[key.ledIndex] = previewColor(settings, x, y, seconds);
    }
    return frame;
}

bool previewAnimates(int effectIndex) {
    return effectIndex != 0 && effectIndex != customEffectIndex && effectIndex != offEffectIndex &&
           effectByIndex(effectIndex) != nullptr;
}

} // namespace F75
