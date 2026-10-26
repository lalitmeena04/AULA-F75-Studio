#pragma once

#include <QByteArray>
#include <QColor>
#include <QList>
#include <QString>
#include <cstdint>
#include <optional>

// Wire format of the AULA F75 (SINO WEALTH "G5KB" family), recovered by static
// analysis of the vendor driver OemDrv.exe. Pure functions; nothing touches
// hardware. Items marked "unverified" have not been exercised on a keyboard.
namespace F75 {

constexpr uint16_t usbVendorId = 0x258A;
constexpr uint16_t usbProductId = 0x010C;
constexpr uint16_t receiverVendorId = 0x3554;
constexpr uint16_t receiverProductId = 0xFA09;
constexpr uint16_t commandUsagePage = 0xFF00;
constexpr uint16_t commandUsage = 0x0001;
constexpr uint16_t receiverStatusUsagePage = 0xFF02;
constexpr uint16_t receiverStatusUsage = 0x0002;
constexpr int receiverStatusLength = 20;

constexpr int reportLength = 0x208;    // incl. report ID
constexpr int payloadLength = 0x200;
constexpr uint8_t reportId = 0x06;     // Fw == 0x18 descriptors: ((Fw != 0x18) - 1 & 0xfd) + 9
constexpr int configLength = 0x80;
constexpr int identityLength = 6;
constexpr int matrixLength = 128;      // KB.ini MatrixLen
constexpr int layerCount = 4;
constexpr int matrixBytes = matrixLength * 4;
constexpr int colorPlaneLength = 126;  // vendor stride per R/G/B plane
constexpr int colorGroupBytes = matrixLength * 3;
constexpr int realtimeLeds = 126;
constexpr int realtimeFrameBytes = realtimeLeds * 3;   // 0x17a, RGB interleaved by LED index
constexpr int realtimePreDelayMs = 7;                  // vendor AccessData delay for command 0x08
constexpr int macroBufferMax = 0x2800;
constexpr int macroNameMaxChars = 30;
constexpr int macroMaxCount = 32;

enum class Command : uint8_t {
    Identity = 0x02,
    Matrix = 0x03,
    Config = 0x04,
    Macro = 0x05,
    ColorGroup = 0x06,
    Realtime = 0x08,      // vendor "music real data": transient per-key frame, nothing persisted
    RgbTable = 0x0A,
    Screen = 0x0B,
    PagedUpload = 0x0C,
    FactoryReset = 0x11,
};
constexpr uint8_t readFlag = 0x80;

struct Frame {
    uint8_t command = 0;
    uint8_t sub = 0;
    uint8_t pageCount = 1;
    uint8_t pageIndex = 0;
    QByteArray payload;   // <= payloadLength
};
QByteArray encodeFrame(const Frame &frame);
std::optional<Frame> decodeFrame(const QByteArray &report);
// Splits a long payload into 512-byte pages, each a full report.
QList<QByteArray> encodePages(Command command, uint8_t sub, const QByteArray &payload);
// Read request header: command|0x80, page geometry for expectedBytes, no payload.
QByteArray readRequest(Command command, uint8_t sub, int expectedBytes, int pageIndex = 0);

struct Variant { QString name; bool wireless; };
std::optional<Variant> identifyVariant(const QByteArray &identity);

// ---- Lighting -------------------------------------------------------------

struct Effect {
    int index;            // 0-based LedOpt row; also the config slot index
    uint8_t hardwareId;   // written to config[0x0a]
    const char *name;
    bool hasSpeed, hasBrightness, hasRandom, hasColor;
};
QList<Effect> effects();
const Effect *effectByIndex(int index);
const Effect *effectByHardwareId(uint8_t id);
constexpr int customEffectIndex = 18;   // "Self-define" (per-key), hw 21
constexpr int offEffectIndex = 19;

// Preset colour indices used in config slots: 0..6 fixed, 7 = colourful/random.
QStringList colorNames();
QColor presetColor(int index);
constexpr int colorCount = 8;

struct SleepPreset { int seconds; uint8_t hardware; };
QList<SleepPreset> sleepPresets();

struct SlotSettings { int brightness = 5; int speed = 2; int color = 7; };

struct Config {
    QByteArray raw;
    bool customMode = false;
    uint8_t effectHardwareId = 0;
    uint8_t sleepHardware = 0;
    SlotSettings slot[20];
};
std::optional<Config> decodeConfig(const QByteArray &raw);
QByteArray encodeEffect(const QByteArray &raw, int effectIndex, SlotSettings settings);
QByteArray encodeSleep(const QByteArray &raw, uint8_t sleepHardware);

// Per-key colours for one colour group (command 0x06, sub = group). Unverified.
QByteArray encodeColorGroup(const QList<QColor> &byLedIndex);
// Transient per-key frame (command 0x08): [r,g,b] per LED index, 378 bytes.
QByteArray encodeRealtimeFrame(const QList<QColor> &byLedIndex);
// Preset colour table (command 0x0A): 7 RGB rows per entry. Unverified.
QByteArray encodeRgbTable(const QList<QColor> &presets, int entryCount, bool padWithSignature);

// ---- Key matrix -----------------------------------------------------------

enum class ActionKind : uint8_t {
    Keyboard = 0x00,   // mods, hid1, hid2
    Consumer = 0x02,   // 16-bit consumer usage
    Macro = 0x03,      // mode, repeat, macro index
    Device = 0x05,     // profile / lighting switch
    Mouse = 0x07,      // button mask
    Special = 0xFF,    // raw 32-bit passthrough (KB.ini param byte order)
};

struct KeyAction {
    ActionKind kind = ActionKind::Keyboard;
    uint8_t modifiers = 0;   // HID: 1 LCtrl 2 LShift 4 LAlt 8 LWin 16 RCtrl 32 RShift 64 RAlt 128 RWin
    uint8_t key = 0;         // HID usage / mouse mask / macro mode / device sub
    uint8_t key2 = 0;        // second HID usage / macro repeat / device arg
    uint16_t usage = 0;      // consumer usage
    uint8_t macroIndex = 0;
    uint32_t raw = 0;        // Special
    bool disabled = false;   // encodes as 0

    uint32_t encode() const;
    static KeyAction decode(uint32_t value);
    QString describe() const;
    bool operator==(const KeyAction &) const = default;
};

// 128 entries of one layer, zero = keep default/no change on device.
QByteArray encodeMatrixLayer(const QList<KeyAction> &byLedIndex, bool trailerSignature);
QList<KeyAction> decodeMatrixLayer(const QByteArray &bytes);

QString hidUsageName(uint8_t usage);
QList<QPair<uint8_t, QString>> hidUsages();                // assignable keyboard usages
QList<QPair<uint16_t, QString>> consumerUsages();          // media / app launch
uint8_t hidUsageFromQtKey(int qtKey, bool &isModifier);   // Qt::Key -> HID usage

// ---- Macros ---------------------------------------------------------------

enum class MacroEventKind : uint8_t { Key = 0, Modifier = 1, MouseButton = 2, Wheel = 3, Reserved = 4, MouseMove = 5 };
struct MacroEvent {
    MacroEventKind kind = MacroEventKind::Key;
    uint8_t code = 0;        // HID usage / modifier usage e0..e7 / mouse mask / signed move
    bool release = false;
    int delayMs = 0;         // 0..0xFFFFF
    bool operator==(const MacroEvent &) const = default;
};
struct Macro {
    QString name;
    QList<MacroEvent> events;
    bool operator==(const Macro &) const = default;
};
QByteArray encodeMacroBody(const Macro &macro);
std::optional<Macro> decodeMacroBody(const QByteArray &body);
// Whole upload: N x {u16 offset, u16 size} table followed by bodies.
QByteArray encodeMacroBuffer(const QList<Macro> &macros);
QList<Macro> decodeMacroBuffer(const QByteArray &buffer);

QString hex(const QByteArray &bytes, int max = -1);
}
