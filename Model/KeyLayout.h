#pragma once

#include <QList>
#include <QRect>
#include <QString>
#include <cstdint>

namespace F75 {

// One physical key of the 75% layout: pixel rectangle on a 660x290 canvas, the
// HID usage it produces by default, and its slot in the 128-entry LED/matrix
// array.
struct KeyInfo {
        int id; // 1-based key number
        QRect rect;
        uint8_t usage; // 0 = non-HID (FN, knob)
        int ledIndex;
        const char* label;
        bool isFn;
        bool isKnob;
};

const QList<KeyInfo>& keyLayout();
const KeyInfo* keyByLedIndex(int ledIndex);
constexpr int layoutWidth = 660;
constexpr int layoutHeight = 290;
} // namespace F75
