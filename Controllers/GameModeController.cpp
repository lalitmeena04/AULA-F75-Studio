#include "GameModeController.h"

#include <windows.h>

#include "Logging.h"

namespace {
// The low-level hook callback has no user pointer; one controller owns the hook.
GameModeController* g_active = nullptr;
} // namespace

struct GameModeHook {
        static LRESULT CALLBACK proc(int code, WPARAM wParam, LPARAM lParam) {
            if(code == HC_ACTION && g_active) {
                const auto* key = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
                // Injected events come from software (macros, remote desktop); let them pass.
                const bool injected = (key->flags & LLKHF_INJECTED) != 0;
                const bool altDown = (key->flags & LLKHF_ALTDOWN) != 0;
                if(!injected && F75::gameModeBlocks(g_active->m_rules, static_cast<int>(key->vkCode), altDown)) {
                    if(wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)
                        g_active->noteBlocked();
                    return 1;
                }
            }
            return CallNextHookEx(nullptr, code, wParam, lParam);
        }
};

GameModeController::GameModeController(QObject* parent) : QObject(parent) {
    m_status = QStringLiteral("Off");
}

GameModeController::~GameModeController() {
    remove();
}

void GameModeController::setEnabled(bool enabled) {
    if(enabled == this->enabled())
        return;
    if(enabled) {
        if(!install())
            return;
        m_blocked = 0;
        m_status = QStringLiteral("On");
    } else {
        remove();
        m_status = QStringLiteral("Off");
    }
    emit changed();
}

void GameModeController::setBlockWin(bool on) {
    if(m_rules.blockWin == on)
        return;
    m_rules.blockWin = on;
    emit changed();
}

void GameModeController::setBlockAltTab(bool on) {
    if(m_rules.blockAltTab == on)
        return;
    m_rules.blockAltTab = on;
    emit changed();
}

void GameModeController::setBlockAltF4(bool on) {
    if(m_rules.blockAltF4 == on)
        return;
    m_rules.blockAltF4 = on;
    emit changed();
}

bool GameModeController::install() {
    if(m_hook)
        return true;
    if(g_active) {
        m_status = QStringLiteral("Another Game Mode hook is active.");
        TRC_WARN("game mode: hook already owned");
        emit changed();
        return false;
    }
    HHOOK hook = SetWindowsHookExW(WH_KEYBOARD_LL, &GameModeHook::proc, GetModuleHandleW(nullptr), 0);
    if(!hook) {
        const DWORD error = GetLastError();
        m_status = QStringLiteral("Could not install the keyboard hook (error %1).").arg(error);
        TRC_ERROR("game mode: SetWindowsHookEx failed, error {}", error);
        emit changed();
        return false;
    }
    m_hook = hook;
    g_active = this;
    TRC_INFO("game mode: hook installed (win={}, altTab={}, altF4={})", m_rules.blockWin, m_rules.blockAltTab,
             m_rules.blockAltF4);
    return true;
}

void GameModeController::remove() {
    if(!m_hook)
        return;
    UnhookWindowsHookEx(static_cast<HHOOK>(m_hook));
    m_hook = nullptr;
    if(g_active == this)
        g_active = nullptr;
    TRC_INFO("game mode: hook removed after blocking {} presses", m_blocked);
}

void GameModeController::noteBlocked() {
    ++m_blocked;
    emit changed();
}
