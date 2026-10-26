#include "Protocol.h"
#include "KeyLayout.h"

#include <QtTest>

using namespace F75;

class ProtocolTest final : public QObject {
    Q_OBJECT
private slots:
    void frameRoundTrip()
    {
        Frame frame;
        frame.command = 0x04;
        frame.sub = 0;
        frame.payload = QByteArray(0x80, 'x');
        const QByteArray report = encodeFrame(frame);
        QCOMPARE(report.size(), reportLength);
        QCOMPARE(static_cast<uint8_t>(report[0]), reportId);
        QCOMPARE(static_cast<uint8_t>(report[6]), 0x80);
        const auto back = decodeFrame(report);
        QVERIFY(back.has_value());
        QCOMPARE(back->payload, frame.payload);
    }

    void pagesSplitLargePayloads()
    {
        const auto pages = encodePages(Command::Matrix, 1, QByteArray(matrixBytes, 'a'));
        QCOMPARE(pages.size(), 1);
        const auto macroPages = encodePages(Command::Macro, 0, QByteArray(1300, 'b'));
        QCOMPARE(macroPages.size(), 3);
        QCOMPARE(static_cast<uint8_t>(macroPages[2][5]), 2);
        QCOMPARE(static_cast<uint8_t>(macroPages[2][6]) | (static_cast<uint8_t>(macroPages[2][7]) << 8), 1300 - 1024);
    }

    void readRequestUsesReadFlag()
    {
        const QByteArray request = readRequest(Command::Identity, 1, identityLength);
        QCOMPARE(static_cast<uint8_t>(request[1]), 0x82);
        QCOMPARE(static_cast<uint8_t>(request[2]), 1);
        QCOMPARE(static_cast<uint8_t>(request[4]), 1);
        QCOMPARE(static_cast<uint8_t>(request[6]), 6);
    }

    void configCodec()
    {
        QByteArray raw(configLength, '\0');
        raw[0x7e] = 0x5a;
        raw[0x7f] = static_cast<char>(0xa5);
        QVERIFY(decodeConfig(raw).has_value());
        const QByteArray next = encodeEffect(raw, 1, {9, 3, 2});
        const auto config = decodeConfig(next);
        QVERIFY(config.has_value());
        QCOMPARE(config->effectHardwareId, uint8_t(2));
        QCOMPARE(config->slot[1].brightness, 9);
        QCOMPARE(config->slot[1].speed, 3);
        QCOMPARE(config->slot[1].color, 2);
        QVERIFY(!config->customMode);
        QVERIFY(decodeConfig(encodeEffect(raw, customEffectIndex, {}))->customMode);
        raw[0x7e] = 0;
        QVERIFY(!decodeConfig(raw).has_value());
    }

    void keyActionEncoding()
    {
        KeyAction ctrlZ;
        ctrlZ.modifiers = 1;
        ctrlZ.key = 0x1d;
        QCOMPARE(ctrlZ.encode(), 0x1d000100u);     // usage in byte 3, as read back from the keyboard
        QCOMPARE(KeyAction::decode(0x2b000400u).describe(), QStringLiteral("Alt + Tab"));   // factory Fn+F3
        QCOMPARE(KeyAction::decode(0x00000400u).describe(), QStringLiteral("Alt"));          // factory Alt key
        KeyAction mute;
        mute.kind = ActionKind::Consumer;
        mute.usage = 0x00e2;
        QCOMPARE(mute.encode(), 0xe2000002u);      // vendor "Mute" constant
        KeyAction media;
        media.kind = ActionKind::Consumer;
        media.usage = 0x0183;
        QCOMPARE(media.encode(), 0x83010002u);
        KeyAction macro;
        macro.kind = ActionKind::Macro;
        macro.key = 1;
        macro.key2 = 3;
        macro.macroIndex = 2;
        QCOMPARE(macro.encode(), 0x02030103u);
        QCOMPARE(KeyAction::decode(0x02030103u), macro);
        QCOMPARE(KeyAction::decode(0xe2000002u).describe(), QStringLiteral("Mute"));
        QVERIFY(KeyAction::decode(0).disabled);
    }

    void matrixLayerRoundTrip()
    {
        QList<KeyAction> actions(matrixLength, KeyAction{});
        for (KeyAction &a : actions) a.disabled = true;
        for (const KeyInfo &key : keyLayout()) {
            if (key.usage == 0) continue;
            KeyAction a;
            a.key = key.usage;
            actions[key.ledIndex] = a;
        }
        const QByteArray bytes = encodeMatrixLayer(actions, true);
        QCOMPARE(bytes.size(), matrixBytes);
        QCOMPARE(static_cast<uint8_t>(bytes[matrixBytes - 1]), 0xa5);
        QCOMPARE(static_cast<uint8_t>(bytes[matrixBytes - 2]), 0x5a);
        const auto back = decodeMatrixLayer(bytes);
        QCOMPARE(back[9].key, uint8_t(0x04));   // A at LED 9
        QVERIFY(back[0].key == 0x29);           // Esc at LED 0
        QCOMPARE(static_cast<uint8_t>(bytes[0 * 4 + 3]), 0x29);   // usage lives in byte 3
    }

    void macroCodec()
    {
        Macro macro;
        macro.name = QStringLiteral("Hi");
        macro.events = {{MacroEventKind::Modifier, 0xe1, false, 0}, {MacroEventKind::Key, 0x0b, false, 20},
                        {MacroEventKind::Key, 0x0b, true, 30}, {MacroEventKind::Modifier, 0xe1, true, 10},
                        {MacroEventKind::Key, 0x0c, false, 0x12345}, {MacroEventKind::Key, 0x0c, true, 5}};
        const QByteArray body = encodeMacroBody(macro);
        QCOMPARE(static_cast<uint8_t>(body[0]), 4);     // "Hi" in UTF-16 = 4 bytes
        QCOMPARE(body.size(), 1 + 4 + 6 * 4);
        QCOMPARE(static_cast<uint8_t>(body[5]), 0x10);  // modifier press, delay 0
        QCOMPARE(static_cast<uint8_t>(body[5 + 8]), 0x80);   // key release
        QCOMPARE(static_cast<uint8_t>(body[5 + 16]), 0x01);  // delay high nibble
        QCOMPARE(static_cast<uint8_t>(body[5 + 17]), 0x23);
        QCOMPARE(static_cast<uint8_t>(body[5 + 18]), 0x45);
        const auto back = decodeMacroBody(body);
        QVERIFY(back.has_value());
        QCOMPARE(*back, macro);

        Macro second;
        second.name = QStringLiteral("Two");
        second.events = {{MacroEventKind::Key, 0x04, false, 1}, {MacroEventKind::Key, 0x04, true, 1}};
        const QByteArray buffer = encodeMacroBuffer({macro, second});
        QCOMPARE(static_cast<uint8_t>(buffer[0]), 8);   // first body right after the 2-entry table
        const auto macros = decodeMacroBuffer(buffer);
        QCOMPARE(macros.size(), 2);
        QCOMPARE(macros[1], second);
    }

    void colorGroupPlanes()
    {
        QList<QColor> frame(matrixLength, QColor());
        frame[9] = QColor(10, 20, 30);
        const QByteArray bytes = encodeColorGroup(frame);
        QCOMPARE(bytes.size(), colorGroupBytes);
        QCOMPARE(static_cast<uint8_t>(bytes[9]), 10);
        QCOMPARE(static_cast<uint8_t>(bytes[colorPlaneLength + 9]), 20);
        QCOMPARE(static_cast<uint8_t>(bytes[2 * colorPlaneLength + 9]), 30);
    }

    void layoutIsConsistent()
    {
        QSet<int> leds;
        for (const KeyInfo &key : keyLayout()) {
            QVERIFY(!leds.contains(key.ledIndex));
            leds.insert(key.ledIndex);
            QVERIFY(key.ledIndex < matrixLength);
        }
        QCOMPARE(leds.size(), 81);
        QVERIFY(identifyVariant(QByteArray::fromHex("0300000000cd")).has_value());
        QVERIFY(!identifyVariant(QByteArray::fromHex("000000000000")).has_value());
    }
};

QTEST_GUILESS_MAIN(ProtocolTest)
#include "ProtocolTest.moc"
