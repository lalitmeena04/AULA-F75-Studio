#include <gtest/gtest.h>

#include <QColor>
#include <QSet>

#include "KeyLayout.h"
#include "LightingPreview.h"
#include "Protocol.h"

using namespace F75;

namespace {
bool isBlack(const QColor& c) {
    return c.red() == 0 && c.green() == 0 && c.blue() == 0;
}
bool anyLit(const QList<QColor>& frame) {
    for(const QColor& c : frame)
        if(!isBlack(c))
            return true;
    return false;
}
} // namespace

TEST(LightingPreviewTest, OffAndCustomRenderBlack) {
    EXPECT_TRUE(isBlack(previewColor({offEffectIndex, 9, 2, 7}, 0.5, 0.5, 1.0)));
    EXPECT_TRUE(isBlack(previewColor({customEffectIndex, 9, 2, 7}, 0.5, 0.5, 1.0)));
    EXPECT_FALSE(anyLit(previewFrame({offEffectIndex, 9, 2, 7}, 3.0)));
}

TEST(LightingPreviewTest, IsDeterministic) {
    const PreviewSettings settings{6, 7, 3, 7};
    EXPECT_EQ(previewColor(settings, 0.3, 0.6, 2.5), previewColor(settings, 0.3, 0.6, 2.5));
    EXPECT_EQ(previewFrame(settings, 4.25), previewFrame(settings, 4.25));
}

TEST(LightingPreviewTest, StaticUsesThePresetColour) {
    for(int colour = 0; colour < colorCount - 1; ++colour) {
        const QColor expected = presetColor(colour);
        const QColor got = previewColor({0, 9, 2, colour}, 0.2, 0.2, 10.0);
        EXPECT_EQ(got.red(), expected.red()) << colour;
        EXPECT_EQ(got.green(), expected.green()) << colour;
        EXPECT_EQ(got.blue(), expected.blue()) << colour;
    }
}

TEST(LightingPreviewTest, StaticDoesNotChangeOverTime) {
    const PreviewSettings settings{0, 9, 2, 7};
    EXPECT_EQ(previewColor(settings, 0.4, 0.4, 0.0), previewColor(settings, 0.4, 0.4, 99.0));
    EXPECT_FALSE(previewAnimates(0));
    EXPECT_TRUE(previewAnimates(1));
    EXPECT_FALSE(previewAnimates(customEffectIndex));
    EXPECT_FALSE(previewAnimates(offEffectIndex));
}

TEST(LightingPreviewTest, BrightnessScalesTheOutput) {
    const QColor dim = previewColor({0, 0, 2, 2}, 0.5, 0.5, 1.0);
    const QColor bright = previewColor({0, 9, 2, 2}, 0.5, 0.5, 1.0);
    EXPECT_LT(dim.red() + dim.green() + dim.blue(), bright.red() + bright.green() + bright.blue());
    EXPECT_FALSE(isBlack(dim));
}

TEST(LightingPreviewTest, EveryHardwareEffectLightsSomeKeyWithinACycle) {
    for(const Effect& effect : effects()) {
        if(effect.index == customEffectIndex || effect.index == offEffectIndex)
            continue;
        bool lit = false;
        for(double t = 0; t < 12.0 && !lit; t += 0.25)
            lit = anyLit(previewFrame({effect.index, 9, 4, 7}, t));
        EXPECT_TRUE(lit) << effect.name;
    }
}

TEST(LightingPreviewTest, FrameCoversEveryKeyAndOnlyKeys) {
    const QList<QColor> frame = previewFrame({0, 9, 2, 7}, 0.0);
    ASSERT_EQ(frame.size(), matrixLength);
    QSet<int> leds;
    for(const KeyInfo& key : keyLayout()) {
        leds.insert(key.ledIndex);
        EXPECT_FALSE(isBlack(frame[key.ledIndex])) << key.label;
    }
    for(int led = 0; led < matrixLength; ++led)
        if(!leds.contains(led))
            EXPECT_TRUE(isBlack(frame[led])) << led;
}

TEST(LightingPreviewTest, ColourlessEffectsIgnoreThePresetColour) {
    // Rainbow (2) has no colour parameter: a solid preset must still give hues.
    const QColor a = previewColor({2, 9, 2, 0}, 0.1, 0.5, 0.0);
    const QColor b = previewColor({2, 9, 2, 0}, 0.1, 0.5, 2.0);
    EXPECT_NE(a, b);
}
