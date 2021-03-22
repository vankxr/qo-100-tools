#include "f1958.h"

float F1958_ATTENUATION[3];

static uint8_t f1958_bit_reverse(uint8_t ubByte)
{
    static const uint8_t ubNibbleRev[] = { 0x0, 0x8, 0x4, 0xC, 0x2, 0xA, 0x6, 0xE, 0x1, 0x9, 0x5, 0xD, 0x3, 0xB, 0x7, 0xF };

    return (ubNibbleRev[ubByte & 0xF] << 4) | ubNibbleRev[ubByte >> 4];
}

uint8_t f1958_init(uint8_t ubID)
{
    if(ubID > 2)
        return 0;

    F1958_ATTENUATION[ubID] = 32.75f;

    return 1;
}

uint8_t f1958_set_attenuation(uint8_t ubID, float fAttenuation)
{
    if(ubID > 2)
        return 0;

    if(fAttenuation < 1.f || fAttenuation > 32.75f)
        return 0;

    uint8_t ubCode = (fAttenuation - 1.f) / .25f;

    F1958_ATTENUATION[ubID] = (float)ubCode * .25f + 1.f;

    ubCode &= 0x7F;
    ubCode = f1958_bit_reverse(ubCode);

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        switch(ubID)
        {
            case F1958_IF_ATT_ID:
                ATT_IF_UNLATCH();
            break;
            case F1958_RF1_ATT_ID:
                ATT_RF1_UNLATCH();
            break;
            case F1958_RF2_ATT_ID:
                ATT_RF2_UNLATCH();
            break;
        }

        usart0_spi_write_byte(ubCode, 1);

        switch(ubID)
        {
            case F1958_IF_ATT_ID:
                ATT_IF_LATCH();
            break;
            case F1958_RF1_ATT_ID:
                ATT_RF1_LATCH();
            break;
            case F1958_RF2_ATT_ID:
                ATT_RF2_LATCH();
            break;
        }
    }

    return 1;
}