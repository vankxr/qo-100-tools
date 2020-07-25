#include "f1951.h"

float F1951_ATTENUATION;

static uint8_t f1951_bit_reverse(uint8_t ubByte)
{
    static const uint8_t ubNibbleRev[] = { 0x0, 0x8, 0x4, 0xC, 0x2, 0xA, 0x6, 0xE, 0x1, 0x9, 0x5, 0xD, 0x3, 0xB, 0x7, 0xF };

    return (ubNibbleRev[ubByte & 0xF] << 4) | ubNibbleRev[ubByte >> 4];
}

uint8_t f1951_init()
{
    F1951_ATTENUATION = 32.5f;

    return 1;
}

uint8_t f1951_set_attenuation(float fAttenuation)
{
    if(fAttenuation < 1.f || fAttenuation > 32.5f)
        return 0;

    uint8_t ubCode = (fAttenuation - 1.f) / .5f;

    F1951_ATTENUATION = (float)ubCode * .5f + 1.f;

    ubCode ^= 0x3F;
    ubCode = (f1951_bit_reverse(ubCode) >> 2) & 0x3F;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        ATT_SELECT();

        usart0_spi_write_byte(ubCode, 1);

        ATT_UNSELECT();
    }

    return 1;
}