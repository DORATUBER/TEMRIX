#pragma once
#include <temrixstd.h>

static constexpr uint32_t HidBootKeyboardReportSize = 8;
static constexpr uint32_t HidBootKeyboardMaxKeyCodes = 6;

struct HidBootKeyboardReport
{
    uint8_t modifierKeys;
    uint8_t reservedByte;
    uint8_t keyCodes[HidBootKeyboardMaxKeyCodes];
};

static inline void HidDecodeBootKeyboardReport(const uint8_t *rawReportBytes, HidBootKeyboardReport &outReport)
{
    outReport.modifierKeys = rawReportBytes[0];
    outReport.reservedByte = rawReportBytes[1];
    for (uint32_t keyCodeIndex = 0; keyCodeIndex < HidBootKeyboardMaxKeyCodes; ++keyCodeIndex)
        outReport.keyCodes[keyCodeIndex] = rawReportBytes[2 + keyCodeIndex];
}

struct HidBootMouseReport
{
    uint8_t buttonState;
    int8_t  deltaX;
    int8_t  deltaY;
};

static inline void HidDecodeBootMouseReport(const uint8_t *rawReportBytes, HidBootMouseReport &outReport)
{
    outReport.buttonState = rawReportBytes[0];
    outReport.deltaX = (int8_t)rawReportBytes[1];
    outReport.deltaY = (int8_t)rawReportBytes[2];
}