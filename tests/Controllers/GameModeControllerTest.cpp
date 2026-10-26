#include <gtest/gtest.h>

#include <QSignalSpy>

#include "GameModeController.h"

TEST(GameModeControllerTest, StartsOffWithEveryRuleEnabled) {
    GameModeController controller;
    EXPECT_FALSE(controller.enabled());
    EXPECT_TRUE(controller.blockWin());
    EXPECT_TRUE(controller.blockAltTab());
    EXPECT_TRUE(controller.blockAltF4());
    EXPECT_EQ(controller.blockedCount(), 0);
    EXPECT_EQ(controller.status(), QStringLiteral("Off"));
}

TEST(GameModeControllerTest, RuleTogglesNotifyAndFeedTheRules) {
    GameModeController controller;
    QSignalSpy changed(&controller, &GameModeController::changed);
    controller.setBlockWin(false);
    controller.setBlockAltTab(false);
    controller.setBlockAltF4(false);
    EXPECT_EQ(changed.count(), 3);
    EXPECT_FALSE(controller.rules().blockWin);
    EXPECT_FALSE(controller.rules().blockAltTab);
    EXPECT_FALSE(controller.rules().blockAltF4);
    controller.setBlockWin(false); // no change, no signal
    EXPECT_EQ(changed.count(), 3);
}

TEST(GameModeControllerTest, HookInstallsAndRemovesCleanly) {
    GameModeController controller;
    QSignalSpy changed(&controller, &GameModeController::changed);
    controller.setEnabled(true);
    if(!controller.enabled())
        GTEST_SKIP() << "Keyboard hook not available in this session: " << controller.status().toStdString();
    EXPECT_EQ(controller.status(), QStringLiteral("On"));
    EXPECT_EQ(changed.count(), 1);

    // A second controller cannot take the hook while the first owns it.
    GameModeController other;
    other.setEnabled(true);
    EXPECT_FALSE(other.enabled());

    controller.setEnabled(false);
    EXPECT_FALSE(controller.enabled());
    EXPECT_EQ(controller.status(), QStringLiteral("Off"));
    EXPECT_EQ(changed.count(), 2);

    other.setEnabled(true);
    EXPECT_TRUE(other.enabled());
}
