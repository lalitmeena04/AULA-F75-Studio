#include "GameModeRules.h"

namespace F75 {

bool gameModeBlocks(const GameModeRules& rules, int virtualKey, bool altDown) {
    if(rules.blockWin && (virtualKey == vkLeftWin || virtualKey == vkRightWin))
        return true;
    if(!altDown)
        return false;
    if(rules.blockAltTab && virtualKey == vkTab)
        return true;
    if(rules.blockAltF4 && virtualKey == vkF4)
        return true;
    return false;
}

} // namespace F75
