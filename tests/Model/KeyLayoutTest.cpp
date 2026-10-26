#include <gtest/gtest.h>

#include <QSet>

#include "KeyLayout.h"
#include "Protocol.h"

using namespace F75;

TEST(KeyLayoutTest, EveryKeyHasAUniqueLedInsideTheMatrix) {
    QSet<int> leds;
    for(const KeyInfo& key : keyLayout()) {
        EXPECT_FALSE(leds.contains(key.ledIndex)) << "duplicate LED " << key.ledIndex;
        leds.insert(key.ledIndex);
        EXPECT_LT(key.ledIndex, matrixLength);
    }
    EXPECT_EQ(leds.size(), 81);
}

TEST(KeyLayoutTest, LookupByLedIndexMatchesTheTable) {
    for(const KeyInfo& key : keyLayout()) {
        const KeyInfo* found = keyByLedIndex(key.ledIndex);
        ASSERT_NE(found, nullptr);
        EXPECT_EQ(found->id, key.id);
    }
    EXPECT_EQ(keyByLedIndex(matrixLength), nullptr);
    EXPECT_EQ(keyByLedIndex(-1), nullptr);
}

TEST(KeyLayoutTest, RectanglesStayInsideTheLayoutCanvas) {
    for(const KeyInfo& key : keyLayout()) {
        EXPECT_GE(key.rect.left(), 0);
        EXPECT_GE(key.rect.top(), 0);
        EXPECT_LE(key.rect.right(), layoutWidth);
        EXPECT_LE(key.rect.bottom(), layoutHeight);
    }
}
