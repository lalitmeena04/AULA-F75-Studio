#pragma once

#include <QObject>
#include <QString>

#include "GameModeRules.h"

// Host-side Game Mode. While enabled, a low-level keyboard hook swallows the
// Windows keys, Alt+Tab and Alt+F4 (each one optional) so they cannot pull
// focus away from a full-screen game. Nothing is written to the keyboard;
// the hook is removed when disabled or when the app exits.
class GameModeController final : public QObject {
        Q_OBJECT
        Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY changed)
        Q_PROPERTY(bool blockWin READ blockWin WRITE setBlockWin NOTIFY changed)
        Q_PROPERTY(bool blockAltTab READ blockAltTab WRITE setBlockAltTab NOTIFY changed)
        Q_PROPERTY(bool blockAltF4 READ blockAltF4 WRITE setBlockAltF4 NOTIFY changed)
        Q_PROPERTY(int blockedCount READ blockedCount NOTIFY changed)
        Q_PROPERTY(QString status READ status NOTIFY changed)

    public:
        explicit GameModeController(QObject* parent = nullptr);
        ~GameModeController() override;

        bool enabled() const {
            return m_hook != nullptr;
        }
        void setEnabled(bool enabled);
        bool blockWin() const {
            return m_rules.blockWin;
        }
        void setBlockWin(bool on);
        bool blockAltTab() const {
            return m_rules.blockAltTab;
        }
        void setBlockAltTab(bool on);
        bool blockAltF4() const {
            return m_rules.blockAltF4;
        }
        void setBlockAltF4(bool on);
        int blockedCount() const {
            return m_blocked;
        }
        QString status() const {
            return m_status;
        }
        const F75::GameModeRules& rules() const {
            return m_rules;
        }

    signals:
        void changed();

    private:
        friend struct GameModeHook;
        bool install();
        void remove();
        void noteBlocked();

        F75::GameModeRules m_rules;
        void* m_hook = nullptr; // HHOOK
        int m_blocked = 0;
        QString m_status;
};
