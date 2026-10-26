#include <gtest/gtest.h>

#include <QSignalSpy>

#include "KeyLayout.h"
#include "LightingPreviewController.h"
#include "Protocol.h"

TEST(LightingPreviewControllerTest, ProducesNoFrameUntilRunning) {
    LightingPreviewController preview;
    preview.setEffectIndex(2);
    EXPECT_TRUE(preview.frame().isEmpty());
    EXPECT_FALSE(preview.running());
}

TEST(LightingPreviewControllerTest, RendersEveryKeyWhenStarted) {
    LightingPreviewController preview;
    preview.setEffectIndex(0); // Static lights every key
    preview.setColor(2);
    QSignalSpy frames(&preview, &LightingPreviewController::frameChanged);
    preview.setRunning(true);
    ASSERT_GE(frames.count(), 1);
    const QVariantMap frame = preview.frame();
    for(const F75::KeyInfo& key : F75::keyLayout())
        EXPECT_TRUE(frame.contains(QString::number(key.ledIndex))) << key.label;
    EXPECT_EQ(frame.size(), F75::keyLayout().size());
    EXPECT_EQ(frame.value(QStringLiteral("9")).toString(), F75::presetColor(2).name());
}

TEST(LightingPreviewControllerTest, ClampsParameters) {
    LightingPreviewController preview;
    preview.setBrightness(42);
    preview.setSpeed(-3);
    preview.setColor(99);
    preview.setFps(1000);
    EXPECT_EQ(preview.brightness(), 9);
    EXPECT_EQ(preview.speed(), 0);
    EXPECT_EQ(preview.color(), F75::colorCount - 1);
    EXPECT_EQ(preview.fps(), 60);
}

TEST(LightingPreviewControllerTest, OffEffectRendersEmptyFrame) {
    LightingPreviewController preview;
    preview.setRunning(true);
    preview.setEffectIndex(F75::offEffectIndex);
    EXPECT_TRUE(preview.frame().isEmpty());
}
