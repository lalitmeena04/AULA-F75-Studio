#include <gtest/gtest.h>

#include <QByteArray>
#include <QColor>
#include <QList>

#include "KeyLayout.h"
#include "Protocol.h"

using namespace F75;

namespace {
uint8_t byteAt(const QByteArray& bytes, int index) {
    return static_cast<uint8_t>(bytes[index]);
}
} // namespace

TEST(ProtocolTest, FrameRoundTrip) {
    Frame frame;
    frame.command = 0x04;
    frame.sub = 0;
    frame.payload = QByteArray(0x80, 'x');
    const QByteArray report = encodeFrame(frame);
    EXPECT_EQ(report.size(), reportLength);
    EXPECT_EQ(byteAt(report, 0), reportId);
    EXPECT_EQ(byteAt(report, 6), 0x80);
    const auto back = decodeFrame(report);
    ASSERT_TRUE(back.has_value());
    EXPECT_EQ(back->payload, frame.payload);
}

TEST(ProtocolTest, PagesSplitLargePayloads) {
    const auto pages = encodePages(Command::Matrix, 1, QByteArray(matrixBytes, 'a'));
    EXPECT_EQ(pages.size(), 1);
    const auto macroPages = encodePages(Command::Macro, 0, QByteArray(1300, 'b'));
    ASSERT_EQ(macroPages.size(), 3);
    EXPECT_EQ(byteAt(macroPages[2], 5), 2);
    EXPECT_EQ(byteAt(macroPages[2], 6) | (byteAt(macroPages[2], 7) << 8), 1300 - 1024);
}

TEST(ProtocolTest, ReadRequestUsesReadFlag) {
    const QByteArray request = readRequest(Command::Identity, 1, identityLength);
    EXPECT_EQ(byteAt(request, 1), 0x82);
    EXPECT_EQ(byteAt(request, 2), 1);
    EXPECT_EQ(byteAt(request, 4), 1);
    EXPECT_EQ(byteAt(request, 6), 6);
}

TEST(ProtocolTest, ConfigCodec) {
    QByteArray raw(configLength, '\0');
    raw[0x7e] = 0x5a;
    raw[0x7f] = static_cast<char>(0xa5);
    EXPECT_TRUE(decodeConfig(raw).has_value());
    const QByteArray next = encodeEffect(raw, 1, {9, 3, 2});
    const auto config = decodeConfig(next);
    ASSERT_TRUE(config.has_value());
    EXPECT_EQ(config->effectHardwareId, uint8_t(2));
    EXPECT_EQ(config->slot[1].brightness, 9);
    EXPECT_EQ(config->slot[1].speed, 3);
    EXPECT_EQ(config->slot[1].color, 2);
    EXPECT_FALSE(config->customMode);
    EXPECT_TRUE(decodeConfig(encodeEffect(raw, customEffectIndex, {}))->customMode);
    raw[0x7e] = 0;
    EXPECT_FALSE(decodeConfig(raw).has_value());
}

TEST(ProtocolTest, KeyActionEncoding) {
    KeyAction ctrlZ;
    ctrlZ.modifiers = 1;
    ctrlZ.key = 0x1d;
    EXPECT_EQ(ctrlZ.encode(), 0x1d000100u); // usage in byte 3, as read back from the keyboard
    EXPECT_EQ(KeyAction::decode(0x2b000400u).describe(), QStringLiteral("Alt + Tab")); // factory Fn+F3
    EXPECT_EQ(KeyAction::decode(0x00000400u).describe(), QStringLiteral("Alt"));       // factory Alt key
    KeyAction mute;
    mute.kind = ActionKind::Consumer;
    mute.usage = 0x00e2;
    EXPECT_EQ(mute.encode(), 0xe2000002u); // consumer Mute as stored on the keyboard
    KeyAction media;
    media.kind = ActionKind::Consumer;
    media.usage = 0x0183;
    EXPECT_EQ(media.encode(), 0x83010002u);
    KeyAction macro;
    macro.kind = ActionKind::Macro;
    macro.key = 1;
    macro.key2 = 3;
    macro.macroIndex = 2;
    EXPECT_EQ(macro.encode(), 0x02030103u);
    EXPECT_TRUE(KeyAction::decode(0x02030103u) == macro);
    EXPECT_EQ(KeyAction::decode(0xe2000002u).describe(), QStringLiteral("Mute"));
    EXPECT_TRUE(KeyAction::decode(0).disabled);
}

TEST(ProtocolTest, MatrixLayerRoundTrip) {
    QList<KeyAction> actions(matrixLength, KeyAction{});
    for(KeyAction& a : actions)
        a.disabled = true;
    for(const KeyInfo& key : keyLayout()) {
        if(key.usage == 0)
            continue;
        KeyAction a;
        a.key = key.usage;
        actions[key.ledIndex] = a;
    }
    const QByteArray bytes = encodeMatrixLayer(actions, true);
    ASSERT_EQ(bytes.size(), matrixBytes);
    EXPECT_EQ(byteAt(bytes, matrixBytes - 1), 0xa5);
    EXPECT_EQ(byteAt(bytes, matrixBytes - 2), 0x5a);
    const auto back = decodeMatrixLayer(bytes);
    EXPECT_EQ(back[9].key, uint8_t(0x04));     // A at LED 9
    EXPECT_EQ(back[0].key, 0x29);              // Esc at LED 0
    EXPECT_EQ(byteAt(bytes, 0 * 4 + 3), 0x29); // usage lives in byte 3
}

TEST(ProtocolTest, MacroCodec) {
    Macro macro;
    macro.name = QStringLiteral("Hi");
    macro.events = {{MacroEventKind::Modifier, 0xe1, false, 0},  {MacroEventKind::Key, 0x0b, false, 20},
                    {MacroEventKind::Key, 0x0b, true, 30},       {MacroEventKind::Modifier, 0xe1, true, 10},
                    {MacroEventKind::Key, 0x0c, false, 0x12345}, {MacroEventKind::Key, 0x0c, true, 5}};
    const QByteArray body = encodeMacroBody(macro);
    EXPECT_EQ(byteAt(body, 0), 4); // "Hi" in UTF-16 = 4 bytes
    EXPECT_EQ(body.size(), 1 + 4 + 6 * 4);
    EXPECT_EQ(byteAt(body, 5), 0x10);      // modifier press, delay 0
    EXPECT_EQ(byteAt(body, 5 + 8), 0x80);  // key release
    EXPECT_EQ(byteAt(body, 5 + 16), 0x01); // delay high nibble
    EXPECT_EQ(byteAt(body, 5 + 17), 0x23);
    EXPECT_EQ(byteAt(body, 5 + 18), 0x45);
    const auto back = decodeMacroBody(body);
    ASSERT_TRUE(back.has_value());
    EXPECT_TRUE(*back == macro);

    Macro second;
    second.name = QStringLiteral("Two");
    second.events = {{MacroEventKind::Key, 0x04, false, 1}, {MacroEventKind::Key, 0x04, true, 1}};
    const QByteArray buffer = encodeMacroBuffer({macro, second});
    EXPECT_EQ(byteAt(buffer, 0), 8); // first body right after the 2-entry table
    const auto macros = decodeMacroBuffer(buffer);
    ASSERT_EQ(macros.size(), 2);
    EXPECT_TRUE(macros[1] == second);
}

TEST(ProtocolTest, ColorGroupPlanes) {
    QList<QColor> frame(matrixLength, QColor());
    frame[9] = QColor(10, 20, 30);
    const QByteArray bytes = encodeColorGroup(frame);
    ASSERT_EQ(bytes.size(), colorGroupBytes);
    EXPECT_EQ(byteAt(bytes, 9), 10);
    EXPECT_EQ(byteAt(bytes, colorPlaneLength + 9), 20);
    EXPECT_EQ(byteAt(bytes, 2 * colorPlaneLength + 9), 30);
}

TEST(ProtocolTest, IdentifiesKnownVariantOnly) {
    EXPECT_TRUE(identifyVariant(QByteArray::fromHex("0300000000cd")).has_value());
    EXPECT_FALSE(identifyVariant(QByteArray::fromHex("000000000000")).has_value());
}
