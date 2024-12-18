#include "KbMakeCodeConverter.h"

#include <iostream>
#include <sstream>
#include <format>

std::wstring KbMakeCodeConverter::convertToCharacter(USHORT makeCode, HKL keyboardLayout,
                                                     bool isShiftPressed, bool isCapsLockEnabled, bool isNumLockEnabled)
{
    static constexpr int    charBufferSize = 4;
    WCHAR                   charBuffer[charBufferSize] = {0};

    keyboardState[VK_SHIFT]     = isShiftPressed ? 0x80 : 0x0;
    keyboardState[VK_CAPITAL]   = isCapsLockEnabled ? 0x01 : 0x0;
    keyboardState[VK_NUMLOCK]   = isNumLockEnabled ? 0x01 : 0x0;

    UINT virtualKey = MapVirtualKeyExW(makeCode, MAPVK_VSC_TO_VK, keyboardLayout);
    if (isNonPrintableSymbolKey(virtualKey)) {
        return getKeyName(makeCode);
    }

    int result = ToUnicodeEx(virtualKey, makeCode, keyboardState, charBuffer, charBufferSize, 0, keyboardLayout);
    if (result > 0) {
        return std::wstring(charBuffer, result);
    }
    else if (result < 0) {
        std::wostringstream strStream;
        strStream << std::format(L"{{Dead Key - SC 0x{:X}, VK 0x{:X}}}", makeCode, virtualKey);
        return strStream.str();
    }
    else {
        return getKeyName(makeCode);
    }
}

bool KbMakeCodeConverter::isNonPrintableSymbolKey(UINT virtualKey) const 
{
    // This keys can be converted to symbols by ToUnicodeEx. E.g., VK_RETURN - '\r'
    // And we shouldn't print them to a console or file as symbols
    return (virtualKey == VK_RETURN
            || virtualKey == VK_ESCAPE
            || virtualKey == VK_BACK
            || virtualKey == VK_TAB
            || virtualKey == VK_SPACE);
}

bool KbMakeCodeConverter::isExtendedKey(UINT scanCode) const
{
    return (scanCode & 0xE000) 
            || scanCode == 0x45;    // This is for processing 0x45 scan code as NumLock and not as Pause
}

std::wstring KbMakeCodeConverter::getKeyName(UINT scanCode) const
{
    static constexpr int    KeyNameBuffSize = 128;
    WCHAR                   keyName[KeyNameBuffSize];

    if (isExtendedKey(scanCode)) {
        // Prepare a scan code for GetKeyNameTextW.
        // So in GetKeyNameTextW, lParam bits 25-31 are 0 and bit 24 is 1 (Extended-key flag)
        scanCode = (scanCode & 0x00FF) | 0x0100;   
    }

    if (GetKeyNameTextW(scanCode << 16, keyName, KeyNameBuffSize)) {
        return L"{" + std::wstring(keyName) + L"}";
    }
    else {
        std::wostringstream strStream;
        strStream << std::format(L"{{Unknown Key - SC 0x{:X}}}", scanCode);
        return strStream.str();
    }
}
