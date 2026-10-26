#pragma once

#include <QColor>
#include <QList>

// Host-side approximation of the keyboard's built-in lighting effects. Pure
// functions: the same settings, key position and time always give the same
// colour. The keyboard runs its own animation; this is only a preview.
namespace F75 {

struct PreviewSettings {
        int effectIndex = 0; // index into effects()
        int brightness = 9;  // 0..9 (config slot range)
        int speed = 2;       // 0..4
        int color = 7;       // 0..6 preset, 7 = colourful
};

// Colour of one key. x and y are the key centre normalised to 0..1 across the
// keyboard (x left to right, y top to bottom). seconds is any monotonic clock.
QColor previewColor(const PreviewSettings& settings, double x, double y, double seconds);

// One frame for every LED index (matrixLength entries, unused slots black),
// using the key centres from keyLayout().
QList<QColor> previewFrame(const PreviewSettings& settings, double seconds);

// True when the effect animates. Static and Off render the same every frame.
bool previewAnimates(int effectIndex);

} // namespace F75
