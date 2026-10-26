#pragma once

// Decides which key presses Game Mode swallows. Pure logic; the Windows hook
// lives in Controllers/GameModeController. Virtual-key values match the
// Win32 VK_* constants so the hook can pass them through unchanged.
namespace F75 {

struct GameModeRules {
        bool blockWin = true;    // Left and right Windows keys
        bool blockAltTab = true; // Alt+Tab task switcher
        bool blockAltF4 = true;  // Alt+F4 close window
        bool operator==(const GameModeRules&) const = default;
};

constexpr int vkTab = 0x09;
constexpr int vkF4 = 0x73;
constexpr int vkLeftWin = 0x5B;
constexpr int vkRightWin = 0x5C;

// True when the key event must not reach the foreground application.
bool gameModeBlocks(const GameModeRules& rules, int virtualKey, bool altDown);

} // namespace F75
