#pragma once

#include <string>
#include <wtypes.h>

class KbMakeCodeConverter {
public:
    std::wstring convertToCharacter(UINT makeCode, HKL keyboardLayout,
                                    bool isShiftPressed, bool isCapsLockEnabled, bool isNumLockEnabled);

private:
    bool isModifierKey(UINT virtualKey) const;
    bool isExtendedKey(UINT scanCode) const;
    std::wstring getKeyName(UINT scanCode) const;

    BYTE keyboardState[256];
};

