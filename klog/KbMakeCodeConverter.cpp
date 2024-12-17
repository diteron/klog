#include "KbMakeCodeConverter.h"

#include <iostream>
#include <sstream>
#include <format>

std::wstring KbMakeCodeConverter::convertToCharacter(UINT makeCode, HKL keyboardLayout,
                                                     bool isShiftPressed, bool isCapsLockEnabled, bool isNumLockEnabled)
{
    static constexpr int    charBufferSize = 4;
    WCHAR                   charBuffer[charBufferSize] = {0};

    keyboardState[VK_SHIFT]     = isShiftPressed ? 0x80 : 0x0;
    keyboardState[VK_CAPITAL]   = isCapsLockEnabled ? 0x01 : 0x0;
    keyboardState[VK_NUMLOCK]   = isNumLockEnabled ? 0x01 : 0x0;

    UINT virtualKey = MapVirtualKeyExW(makeCode, MAPVK_VSC_TO_VK, keyboardLayout);
    if (isModifierKey(virtualKey)) {
        return getKeyName(makeCode);
    }

    int result = ToUnicodeEx(virtualKey, makeCode, keyboardState, charBuffer, charBufferSize, 0, keyboardLayout);
    if (result > 0) {
        return std::wstring(charBuffer, result);
    }
    else if (result < 0) {
        std::wostringstream strStream;
        strStream << std::format(L"{{Dead Key - SC {:#X}, VK {:#X}}}", makeCode, virtualKey);
        return strStream.str();
    }
    else {
        return getKeyName(makeCode);
    }
}

bool KbMakeCodeConverter::isModifierKey(UINT virtualKey) const 
{
    return (virtualKey == VK_RETURN
            || virtualKey == VK_ESCAPE
            || virtualKey == VK_BACK
            || virtualKey == VK_TAB
            || virtualKey == VK_SPACE);
}

bool KbMakeCodeConverter::isExtendedKey(UINT scanCode) const
{
    return (scanCode & 0xE000) 
            || scanCode == 0x45;    // Process 0x45 as NumLock, not Pause
}

std::wstring KbMakeCodeConverter::getKeyName(UINT scanCode) const
{
    static constexpr int    KeyNameBuffSize = 128;
    WCHAR                   keyName[KeyNameBuffSize];

    if (isExtendedKey(scanCode)) {
        scanCode ^= 0xE100;
    }

    if (GetKeyNameTextW(scanCode << 16, keyName, KeyNameBuffSize)) {
        return L"{" + std::wstring(keyName) + L"}";
    }
    else {
        std::wostringstream strStream;
        strStream << std::format(L"{{Unknown Key - SC {:#X}}}", scanCode);
        return strStream.str();
    }
}
