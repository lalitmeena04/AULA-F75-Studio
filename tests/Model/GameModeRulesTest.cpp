#include <gtest/gtest.h>

#include "GameModeRules.h"

using namespace F75;

TEST(GameModeRulesTest, DefaultsBlockWinAltTabAndAltF4) {
    const GameModeRules rules;
    EXPECT_TRUE(gameModeBlocks(rules, vkLeftWin, false));
    EXPECT_TRUE(gameModeBlocks(rules, vkRightWin, false));
    EXPECT_TRUE(gameModeBlocks(rules, vkTab, true));
    EXPECT_TRUE(gameModeBlocks(rules, vkF4, true));
}

TEST(GameModeRulesTest, TabAndF4PassWithoutAlt) {
    const GameModeRules rules;
    EXPECT_FALSE(gameModeBlocks(rules, vkTab, false));
    EXPECT_FALSE(gameModeBlocks(rules, vkF4, false));
}

TEST(GameModeRulesTest, OrdinaryKeysAlwaysPass) {
    const GameModeRules rules;
    for(int vk : {0x41, 0x20, 0x0D, 0x1B, 0x70}) { // A, Space, Enter, Esc, F1
        EXPECT_FALSE(gameModeBlocks(rules, vk, false)) << vk;
        EXPECT_FALSE(gameModeBlocks(rules, vk, true)) << vk;
    }
}

TEST(GameModeRulesTest, EachRuleCanBeDisabledIndependently) {
    GameModeRules rules;
    rules.blockWin = false;
    EXPECT_FALSE(gameModeBlocks(rules, vkLeftWin, false));
    EXPECT_TRUE(gameModeBlocks(rules, vkTab, true));

    rules = GameModeRules{};
    rules.blockAltTab = false;
    EXPECT_FALSE(gameModeBlocks(rules, vkTab, true));
    EXPECT_TRUE(gameModeBlocks(rules, vkF4, true));

    rules = GameModeRules{};
    rules.blockAltF4 = false;
    EXPECT_FALSE(gameModeBlocks(rules, vkF4, true));
    EXPECT_TRUE(gameModeBlocks(rules, vkRightWin, false));
}
