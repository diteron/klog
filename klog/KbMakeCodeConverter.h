#pragma once

#include <string>
#include <wtypes.h>

class KbMakeCodeConverter {
public:
    std::wstring convertToCharacter(USHORT makeCode, HKL keyboardLayout,
                                    bool isShiftPressed, bool isCapsLockEnabled, bool isNumLockEnabled);

private:
    bool isNonPrintableSymbolKey(UINT virtualKey) const;
    bool isExtendedKey(UINT scanCode) const;
    std::wstring getKeyName(UINT scanCode) const;

    BYTE keyboardState[256];
};

