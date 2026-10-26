#include "Protocol.h"

#include <QtEndian>
#include <algorithm>
#include <array>

namespace F75 {
namespace {
// KB.ini LedOptN rows (hw, effectId, speed, light, direct, random, color) with
// the matching text.xml tc_kb_ledN labels.
constexpr std::array<Effect, 20> effectTable{{
    {0, 1, "Static", false, true, true, true},
    {1, 2, "Breathing", true, true, true, true},
    {2, 3, "Rainbow", true, true, false, false},
    {3, 4, "Flash Away", true, true, true, true},
    {4, 5, "Raindrops", true, true, true, true},
    {5, 6, "Rainbow Wheel", true, true, true, true},
    {6, 7, "Ripples", true, true, true, true},
    {7, 8, "Stars Twinkle", true, true, true, true},
    {8, 9, "Shadow Disappear", true, true, true, true},
    {9, 10, "Retro Snake", true, true, true, true},
    {10, 11, "Neon Stream", true, true, true, true},
    {11, 12, "Reaction", true, true, true, true},
    {12, 13, "Sine Wave", true, true, true, true},
    {13, 14, "Retinue Scanning", true, true, true, true},
    {14, 15, "Rotating Windmill", true, true, false, false},
    {15, 16, "Colorful Waterfall", true, true, false, false},
    {16, 17, "Blossoming", true, true, false, false},
    {17, 18, "Rotating Storm", true, true, true, true},
    {18, 21, "Custom (per-key)", false, false, false, false},
    {19, 0, "Off", false, false, false, false},
}};

constexpr std::array<const char *, colorCount> colorTable{
    "Red", "Green", "Blue", "Cyan", "Magenta", "Yellow", "White", "Colorful"};
// Order follows the driver's COLORREF->index switch for Fw 0x18 devices.
constexpr std::array<QRgb, colorCount> colorRgb{
    0xffff0000, 0xff00ff00, 0xff0000ff, 0xff00ffff, 0xffff00ff, 0xffffff00, 0xffffffff, 0xff7f7f7f};

constexpr std::array<SleepPreset, 10> sleepTable{{
    {0, 0}, {60, 2}, {90, 3}, {120, 4}, {180, 6}, {240, 8}, {300, 10}, {600, 20}, {900, 30}, {1200, 40}}};

constexpr int offCustomMode = 0x09;
constexpr int offEffect = 0x0a;
constexpr int offSleep = 0x18;
constexpr int offSlots = 0x3a;
constexpr int offSignature = 0x7e;

struct UsageName { uint8_t usage; const char *name; };
constexpr std::array<UsageName, 108> usageNames{{
    {0x04, "A"}, {0x05, "B"}, {0x06, "C"}, {0x07, "D"}, {0x08, "E"}, {0x09, "F"}, {0x0A, "G"},
    {0x0B, "H"}, {0x0C, "I"}, {0x0D, "J"}, {0x0E, "K"}, {0x0F, "L"}, {0x10, "M"}, {0x11, "N"}, {0x12, "O"},
    {0x13, "P"}, {0x14, "Q"}, {0x15, "R"}, {0x16, "S"}, {0x17, "T"}, {0x18, "U"}, {0x19, "V"}, {0x1A, "W"},
    {0x1B, "X"}, {0x1C, "Y"}, {0x1D, "Z"}, {0x1E, "1"}, {0x1F, "2"}, {0x20, "3"}, {0x21, "4"}, {0x22, "5"},
    {0x23, "6"}, {0x24, "7"}, {0x25, "8"}, {0x26, "9"}, {0x27, "0"}, {0x28, "Enter"}, {0x29, "Esc"},
    {0x2A, "Backspace"}, {0x2B, "Tab"}, {0x2C, "Space"}, {0x2D, "-"}, {0x2E, "="}, {0x2F, "["}, {0x30, "]"},
    {0x31, "\\"}, {0x33, ";"}, {0x34, "'"}, {0x35, "`"}, {0x36, ","}, {0x37, "."}, {0x38, "/"},
    {0x39, "Caps Lock"}, {0x3A, "F1"}, {0x3B, "F2"}, {0x3C, "F3"}, {0x3D, "F4"}, {0x3E, "F5"}, {0x3F, "F6"},
    {0x40, "F7"}, {0x41, "F8"}, {0x42, "F9"}, {0x43, "F10"}, {0x44, "F11"}, {0x45, "F12"},
    {0x46, "Print Screen"}, {0x47, "Scroll Lock"}, {0x48, "Pause"}, {0x49, "Insert"}, {0x4A, "Home"},
    {0x4B, "Page Up"}, {0x4C, "Delete"}, {0x4D, "End"}, {0x4E, "Page Down"}, {0x4F, "Right"}, {0x50, "Left"},
    {0x51, "Down"}, {0x52, "Up"}, {0x53, "Num Lock"}, {0x54, "Num /"}, {0x55, "Num *"}, {0x56, "Num -"},
    {0x57, "Num +"}, {0x58, "Num Enter"}, {0x59, "Num 1"}, {0x5A, "Num 2"}, {0x5B, "Num 3"}, {0x5C, "Num 4"},
    {0x5D, "Num 5"}, {0x5E, "Num 6"}, {0x5F, "Num 7"}, {0x60, "Num 8"}, {0x61, "Num 9"}, {0x62, "Num 0"},
    {0x63, "Num ."}, {0x64, "Intl \\"}, {0x65, "Menu"}, {0x68, "F13"}, {0x69, "F14"}, {0x6A, "F15"},
    {0xE0, "Left Ctrl"}, {0xE1, "Left Shift"}, {0xE2, "Left Alt"}, {0xE3, "Left Win"}, {0xE4, "Right Ctrl"},
    {0xE5, "Right Shift"}, {0xE6, "Right Alt"}, {0xE7, "Right Win"},
}};

struct ConsumerName { uint16_t usage; const char *name; };
constexpr std::array<ConsumerName, 20> consumerNames{{
    {0x00CD, "Play / Pause"}, {0x00B5, "Next Track"}, {0x00B6, "Previous Track"}, {0x00B7, "Stop"},
    {0x00E9, "Volume Up"}, {0x00EA, "Volume Down"}, {0x00E2, "Mute"}, {0x006F, "Brightness Up"},
    {0x0070, "Brightness Down"}, {0x018A, "Email"}, {0x0192, "Calculator"}, {0x0194, "My Computer"},
    {0x0183, "Media Player"}, {0x0221, "Web Search"}, {0x0223, "Browser Home"}, {0x0224, "Browser Back"},
    {0x0225, "Browser Forward"}, {0x0226, "Browser Stop"}, {0x0227, "Browser Refresh"}, {0x022A, "Favorites"},
}};

const char *modifierName(int bit)
{
    static constexpr const char *names[] = {"Ctrl", "Shift", "Alt", "Win", "RCtrl", "RShift", "RAlt", "RWin"};
    return names[bit];
}
}

QByteArray encodeFrame(const Frame &frame)
{
    QByteArray report(reportLength, '\0');
    const int length = std::min<int>(static_cast<int>(frame.payload.size()), payloadLength);
    report[0] = static_cast<char>(reportId);
    report[1] = static_cast<char>(frame.command);
    report[2] = static_cast<char>(frame.sub);
    report[4] = static_cast<char>(frame.pageCount);
    report[5] = static_cast<char>(frame.pageIndex);
    report[6] = static_cast<char>(length & 0xff);
    report[7] = static_cast<char>((length >> 8) & 0xff);
    if (length > 0)
        report.replace(8, length, frame.payload.constData(), length);
    return report;
}

std::optional<Frame> decodeFrame(const QByteArray &report)
{
    if (report.size() != reportLength || static_cast<uint8_t>(report[0]) != reportId)
        return std::nullopt;
    Frame frame;
    frame.command = static_cast<uint8_t>(report[1]);
    frame.sub = static_cast<uint8_t>(report[2]);
    frame.pageCount = static_cast<uint8_t>(report[4]);
    frame.pageIndex = static_cast<uint8_t>(report[5]);
    const int length = static_cast<uint8_t>(report[6]) | (static_cast<uint8_t>(report[7]) << 8);
    frame.payload = report.mid(8, std::min(length, payloadLength));
    return frame;
}

QList<QByteArray> encodePages(Command command, uint8_t sub, const QByteArray &payload)
{
    QList<QByteArray> pages;
    const int count = std::max(1, static_cast<int>((payload.size() + payloadLength - 1) / payloadLength));
    for (int i = 0; i < count; ++i) {
        Frame frame;
        frame.command = static_cast<uint8_t>(command);
        frame.sub = sub;
        frame.pageCount = static_cast<uint8_t>(count);
        frame.pageIndex = static_cast<uint8_t>(i);
        frame.payload = payload.mid(i * payloadLength, payloadLength);
        pages.append(encodeFrame(frame));
    }
    return pages;
}

QByteArray readRequest(Command command, uint8_t sub, int expectedBytes, int pageIndex)
{
    Frame frame;
    frame.command = static_cast<uint8_t>(command) | readFlag;
    frame.sub = sub;
    frame.pageCount = static_cast<uint8_t>((expectedBytes + payloadLength - 1) / payloadLength);
    frame.pageIndex = static_cast<uint8_t>(pageIndex);
    QByteArray report = encodeFrame(frame);
    const int remaining = std::min(payloadLength, expectedBytes - pageIndex * payloadLength);
    report[6] = static_cast<char>(remaining & 0xff);
    report[7] = static_cast<char>((remaining >> 8) & 0xff);
    return report;
}

std::optional<Variant> identifyVariant(const QByteArray &identity)
{
    if (identity.size() != identityLength)
        return std::nullopt;
    if (identity == QByteArray::fromHex("0300000000cd")) return Variant{QStringLiteral("AULA F75"), true};
    if (identity == QByteArray::fromHex("030000000191")) return Variant{QStringLiteral("AULA F75 KR"), true};
    if (identity == QByteArray::fromHex("0300000000e1")) return Variant{QStringLiteral("AULA F75 Wired"), false};
    return std::nullopt;
}

QList<Effect> effects()
{
    return QList<Effect>(effectTable.begin(), effectTable.end());
}

const Effect *effectByIndex(int index)
{
    return index >= 0 && index < static_cast<int>(effectTable.size()) ? &effectTable[static_cast<size_t>(index)] : nullptr;
}

const Effect *effectByHardwareId(uint8_t id)
{
    for (const auto &effect : effectTable)
        if (effect.hardwareId == id)
            return &effect;
    return nullptr;
}

QStringList colorNames()
{
    QStringList list;
    for (const char *name : colorTable)
        list << QLatin1String(name);
    return list;
}

QColor presetColor(int index)
{
    return index >= 0 && index < colorCount ? QColor::fromRgb(colorRgb[static_cast<size_t>(index)]) : QColor();
}

QList<SleepPreset> sleepPresets()
{
    return QList<SleepPreset>(sleepTable.begin(), sleepTable.end());
}

std::optional<Config> decodeConfig(const QByteArray &raw)
{
    if (raw.size() != configLength)
        return std::nullopt;
    if (static_cast<uint8_t>(raw[offSignature]) != 0x5a || static_cast<uint8_t>(raw[offSignature + 1]) != 0xa5)
        return std::nullopt;
    Config config;
    config.raw = raw;
    config.customMode = raw[offCustomMode] != 0;
    config.effectHardwareId = static_cast<uint8_t>(raw[offEffect]);
    config.sleepHardware = static_cast<uint8_t>(raw[offSleep]);
    for (int i = 0; i < 20; ++i) {
        const auto light = static_cast<uint8_t>(raw[offSlots + 2 * i]);
        const auto packed = static_cast<uint8_t>(raw[offSlots + 2 * i + 1]);
        config.slot[i] = {light, packed >> 4, packed & 0x0f};
    }
    return config;
}

QByteArray encodeEffect(const QByteArray &raw, int effectIndex, SlotSettings settings)
{
    QByteArray out = raw;
    const Effect *effect = effectByIndex(effectIndex);
    if (out.size() != configLength || !effect)
        return {};
    out[offCustomMode] = static_cast<char>(effectIndex == customEffectIndex ? 1 : 0);
    out[offEffect] = static_cast<char>(effect->hardwareId);
    const int brightness = std::clamp(settings.brightness, 0, 9);
    const int speed = std::clamp(settings.speed, 0, 4);
    const int color = std::clamp(settings.color, 0, colorCount - 1);
    out[offSlots + 2 * effectIndex] = static_cast<char>(brightness);
    out[offSlots + 2 * effectIndex + 1] = static_cast<char>((speed << 4) | color);
    return out;
}

QByteArray encodeSleep(const QByteArray &raw, uint8_t sleepHardware)
{
    QByteArray out = raw;
    if (out.size() != configLength)
        return {};
    out[offSleep] = static_cast<char>(sleepHardware);
    return out;
}

QByteArray encodeColorGroup(const QList<QColor> &byLedIndex)
{
    // Three planes (R, G, B) with the vendor's 126-byte stride, sent as 384 bytes.
    QByteArray out(colorGroupBytes, '\0');
    for (int led = 0; led < std::min<int>(static_cast<int>(byLedIndex.size()), colorPlaneLength); ++led) {
        const QColor c = byLedIndex[led];
        if (!c.isValid())
            continue;
        out[led] = static_cast<char>(c.red());
        out[colorPlaneLength + led] = static_cast<char>(c.green());
        out[2 * colorPlaneLength + led] = static_cast<char>(c.blue());
    }
    return out;
}

QByteArray encodeRealtimeFrame(const QList<QColor> &byLedIndex)
{
    QByteArray out(realtimeFrameBytes, '\0');
    for (int led = 0; led < std::min<int>(static_cast<int>(byLedIndex.size()), realtimeLeds); ++led) {
        const QColor c = byLedIndex[led];
        if (!c.isValid())
            continue;
        out[led * 3] = static_cast<char>(c.red());
        out[led * 3 + 1] = static_cast<char>(c.green());
        out[led * 3 + 2] = static_cast<char>(c.blue());
    }
    return out;
}

QByteArray encodeRgbTable(const QList<QColor> &presets, int entryCount, bool padWithSignature)
{
    constexpr int rowBytes = 0x15;
    QByteArray table((entryCount + 1) * rowBytes, '\0');
    for (int entry = 0; entry < entryCount; ++entry)
        for (int c = 0; c < 7 && c < presets.size(); ++c) {
            const int at = entry * rowBytes + c * 3;
            table[at] = static_cast<char>(presets[c].red());
            table[at + 1] = static_cast<char>(presets[c].green());
            table[at + 2] = static_cast<char>(presets[c].blue());
        }
    if (!padWithSignature)
        return table;
    QByteArray padded(payloadLength, '\0');
    padded.replace(0, table.size(), table);
    padded[0x1fa] = 0x5a;
    padded[0x1fb] = static_cast<char>(0xa5);
    return padded;
}

uint32_t KeyAction::encode() const
{
    if (disabled)
        return 0;
    auto pack = [](uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3) {
        return uint32_t(b0) | (uint32_t(b1) << 8) | (uint32_t(b2) << 16) | (uint32_t(b3) << 24);
    };
    switch (kind) {
    case ActionKind::Keyboard: return pack(0x00, modifiers, key2, key);
    case ActionKind::Consumer: return pack(0x02, 0x00, uint8_t(usage >> 8), uint8_t(usage & 0xff));
    case ActionKind::Macro: return pack(0x03, key, key2, macroIndex);
    case ActionKind::Device: return pack(0x05, key, 0x00, key2);
    case ActionKind::Mouse: return pack(0x07, 0x00, 0x00, key);
    case ActionKind::Special: return raw;
    }
    return 0;
}

KeyAction KeyAction::decode(uint32_t value)
{
    KeyAction a;
    const uint8_t b0 = value & 0xff, b1 = (value >> 8) & 0xff, b2 = (value >> 16) & 0xff, b3 = (value >> 24) & 0xff;
    if (value == 0) { a.disabled = true; return a; }
    switch (b0) {
    case 0x00: a.kind = ActionKind::Keyboard; a.modifiers = b1; a.key2 = b2; a.key = b3; break;
    case 0x02: a.kind = ActionKind::Consumer; a.usage = uint16_t((b2 << 8) | b3); break;
    case 0x03: a.kind = ActionKind::Macro; a.key = b1; a.key2 = b2; a.macroIndex = b3; break;
    case 0x05: a.kind = ActionKind::Device; a.key = b1; a.key2 = b3; break;
    case 0x07: a.kind = ActionKind::Mouse; a.key = b3; break;
    default: a.kind = ActionKind::Special; a.raw = value; break;
    }
    return a;
}

QString KeyAction::describe() const
{
    if (disabled)
        return QStringLiteral("Disabled");
    switch (kind) {
    case ActionKind::Keyboard: {
        QStringList parts;
        for (int bit = 0; bit < 8; ++bit)
            if (modifiers & (1 << bit))
                parts << QLatin1String(modifierName(bit));
        if (key) parts << hidUsageName(key);
        if (key2) parts << hidUsageName(key2);
        return parts.isEmpty() ? QStringLiteral("None") : parts.join(QStringLiteral(" + "));
    }
    case ActionKind::Consumer:
        for (const auto &c : consumerNames)
            if (c.usage == usage)
                return QLatin1String(c.name);
        return QStringLiteral("Consumer 0x%1").arg(usage, 4, 16, QLatin1Char('0'));
    case ActionKind::Macro:
        return QStringLiteral("Macro %1 (%2)").arg(macroIndex + 1)
            .arg(key == 2 ? QStringLiteral("while held") : key == 4 ? QStringLiteral("toggle")
                                                                      : QStringLiteral("x%1").arg(key2));
    case ActionKind::Device:
        return key == 5 ? QStringLiteral("Profile %1").arg(key2 + 1) : QStringLiteral("Device fn %1").arg(key);
    case ActionKind::Mouse:
        return key == 1 ? QStringLiteral("Left Click") : key == 2 ? QStringLiteral("Right Click")
             : key == 4 ? QStringLiteral("Middle Click") : QStringLiteral("Device fn 0x%1").arg(key, 2, 16, QLatin1Char('0'));
    case ActionKind::Special:
        return QStringLiteral("Raw 0x%1").arg(raw, 8, 16, QLatin1Char('0'));
    }
    return {};
}

QByteArray encodeMatrixLayer(const QList<KeyAction> &byLedIndex, bool trailerSignature)
{
    QByteArray out(matrixBytes, '\0');
    for (int i = 0; i < std::min<int>(static_cast<int>(byLedIndex.size()), matrixLength); ++i)
        qToLittleEndian<quint32>(byLedIndex[i].encode(), out.data() + i * 4);
    if (trailerSignature)
        qToLittleEndian<quint32>(0xa55a0000u, out.data() + matrixBytes - 4);
    return out;
}

QList<KeyAction> decodeMatrixLayer(const QByteArray &bytes)
{
    QList<KeyAction> actions;
    for (int i = 0; i + 4 <= bytes.size() && i < matrixBytes; i += 4)
        actions.append(KeyAction::decode(qFromLittleEndian<quint32>(bytes.constData() + i)));
    return actions;
}

QString hidUsageName(uint8_t usage)
{
    for (const auto &entry : usageNames)
        if (entry.usage == usage)
            return QLatin1String(entry.name);
    return QStringLiteral("0x%1").arg(usage, 2, 16, QLatin1Char('0'));
}

QList<QPair<uint8_t, QString>> hidUsages()
{
    QList<QPair<uint8_t, QString>> list;
    for (const auto &entry : usageNames)
        list.append({entry.usage, QLatin1String(entry.name)});
    return list;
}

QList<QPair<uint16_t, QString>> consumerUsages()
{
    QList<QPair<uint16_t, QString>> list;
    for (const auto &entry : consumerNames)
        list.append({entry.usage, QLatin1String(entry.name)});
    return list;
}

uint8_t hidUsageFromQtKey(int qtKey, bool &isModifier)
{
    isModifier = false;
    if (qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z) return uint8_t(0x04 + (qtKey - Qt::Key_A));
    if (qtKey >= Qt::Key_1 && qtKey <= Qt::Key_9) return uint8_t(0x1E + (qtKey - Qt::Key_1));
    if (qtKey >= Qt::Key_F1 && qtKey <= Qt::Key_F12) return uint8_t(0x3A + (qtKey - Qt::Key_F1));
    switch (qtKey) {
    case Qt::Key_0: return 0x27;
    case Qt::Key_Return: case Qt::Key_Enter: return 0x28;
    case Qt::Key_Escape: return 0x29;
    case Qt::Key_Backspace: return 0x2A;
    case Qt::Key_Tab: case Qt::Key_Backtab: return 0x2B;
    case Qt::Key_Space: return 0x2C;
    case Qt::Key_Minus: return 0x2D;
    case Qt::Key_Equal: return 0x2E;
    case Qt::Key_BracketLeft: return 0x2F;
    case Qt::Key_BracketRight: return 0x30;
    case Qt::Key_Backslash: return 0x31;
    case Qt::Key_Semicolon: return 0x33;
    case Qt::Key_Apostrophe: return 0x34;
    case Qt::Key_QuoteLeft: return 0x35;
    case Qt::Key_Comma: return 0x36;
    case Qt::Key_Period: return 0x37;
    case Qt::Key_Slash: return 0x38;
    case Qt::Key_CapsLock: return 0x39;
    case Qt::Key_Print: return 0x46;
    case Qt::Key_ScrollLock: return 0x47;
    case Qt::Key_Pause: return 0x48;
    case Qt::Key_Insert: return 0x49;
    case Qt::Key_Home: return 0x4A;
    case Qt::Key_PageUp: return 0x4B;
    case Qt::Key_Delete: return 0x4C;
    case Qt::Key_End: return 0x4D;
    case Qt::Key_PageDown: return 0x4E;
    case Qt::Key_Right: return 0x4F;
    case Qt::Key_Left: return 0x50;
    case Qt::Key_Down: return 0x51;
    case Qt::Key_Up: return 0x52;
    case Qt::Key_NumLock: return 0x53;
    case Qt::Key_Menu: return 0x65;
    case Qt::Key_Control: isModifier = true; return 0xE0;
    case Qt::Key_Shift: isModifier = true; return 0xE1;
    case Qt::Key_Alt: isModifier = true; return 0xE2;
    case Qt::Key_Meta: isModifier = true; return 0xE3;
    case Qt::Key_AltGr: isModifier = true; return 0xE6;
    }
    return 0;
}

QByteArray encodeMacroBody(const Macro &macro)
{
    QByteArray out;
    const QString name = macro.name.left(macroNameMaxChars);
    const auto utf16 = QByteArray(reinterpret_cast<const char *>(name.utf16()), name.size() * 2);
    out.append(static_cast<char>(utf16.size()));
    out.append(utf16);
    for (const MacroEvent &event : macro.events) {
        const int delay = std::clamp(event.delayMs, 0, 0xfffff);
        uint8_t b0 = uint8_t((uint8_t(event.kind) << 4) | ((delay >> 16) & 0x0f));
        const bool keyLike = event.kind == MacroEventKind::Key || event.kind == MacroEventKind::Modifier ||
                             event.kind == MacroEventKind::MouseButton;
        if (keyLike && event.release)
            b0 |= 0x80;
        out.append(static_cast<char>(b0));
        out.append(static_cast<char>((delay >> 8) & 0xff));
        out.append(static_cast<char>(delay & 0xff));
        out.append(static_cast<char>(event.code));
    }
    return out;
}

std::optional<Macro> decodeMacroBody(const QByteArray &body)
{
    if (body.isEmpty())
        return std::nullopt;
    const int nameBytes = static_cast<uint8_t>(body[0]);
    if (1 + nameBytes > body.size() || (nameBytes & 1))
        return std::nullopt;
    Macro macro;
    macro.name = QString::fromUtf16(reinterpret_cast<const char16_t *>(body.constData() + 1), nameBytes / 2);
    for (int at = 1 + nameBytes; at + 4 <= body.size(); at += 4) {
        const auto b0 = static_cast<uint8_t>(body[at]);
        MacroEvent event;
        event.kind = static_cast<MacroEventKind>((b0 >> 4) & 0x07);
        event.release = (b0 & 0x80) != 0;
        event.delayMs = ((b0 & 0x0f) << 16) | (static_cast<uint8_t>(body[at + 1]) << 8) | static_cast<uint8_t>(body[at + 2]);
        event.code = static_cast<uint8_t>(body[at + 3]);
        macro.events.append(event);
    }
    return macro;
}

QByteArray encodeMacroBuffer(const QList<Macro> &macros)
{
    const int count = std::min<int>(static_cast<int>(macros.size()), macroMaxCount);
    QList<QByteArray> bodies;
    for (int i = 0; i < count; ++i)
        bodies.append(encodeMacroBody(macros[i]));
    QByteArray out(count * 4, '\0');
    int offset = count * 4;
    for (int i = 0; i < count; ++i) {
        qToLittleEndian<quint16>(static_cast<quint16>(offset), out.data() + i * 4);
        qToLittleEndian<quint16>(static_cast<quint16>(bodies[i].size()), out.data() + i * 4 + 2);
        offset += static_cast<int>(bodies[i].size());
    }
    for (const QByteArray &body : bodies)
        out.append(body);
    return out;
}

QList<Macro> decodeMacroBuffer(const QByteArray &buffer)
{
    QList<Macro> macros;
    if (buffer.size() < 4)
        return macros;
    const int firstOffset = qFromLittleEndian<quint16>(buffer.constData());
    const int count = firstOffset / 4;
    if (count <= 0 || count > macroMaxCount || firstOffset > buffer.size())
        return macros;
    for (int i = 0; i < count; ++i) {
        const int offset = qFromLittleEndian<quint16>(buffer.constData() + i * 4);
        const int size = qFromLittleEndian<quint16>(buffer.constData() + i * 4 + 2);
        if (offset + size > buffer.size())
            break;
        if (auto macro = decodeMacroBody(buffer.mid(offset, size)))
            macros.append(*macro);
    }
    return macros;
}

QString hex(const QByteArray &bytes, int max)
{
    QString text;
    const int n = max < 0 ? static_cast<int>(bytes.size()) : std::min<int>(max, static_cast<int>(bytes.size()));
    for (int i = 0; i < n; ++i) {
        if (i) text += QLatin1Char(' ');
        text += QStringLiteral("%1").arg(static_cast<uint8_t>(bytes[i]), 2, 16, QLatin1Char('0'));
    }
    if (n < bytes.size())
        text += QStringLiteral(" …");
    return text;
}

}
