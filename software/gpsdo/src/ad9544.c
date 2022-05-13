#include "ad9544.h"

static uint8_t ad9544_read_register(uint8_t ubRegister)
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        i2c2_write_byte(AD9544_I2C_ADDR, ubRegister, I2C_RESTART);
        return i2c2_read_byte(AD9544_I2C_ADDR, I2C_STOP);
    }
}
static void ad9544_write_register(uint8_t ubRegister, uint8_t ubValue)
{
    uint8_t pubBuffer[2];

    pubBuffer[0] = ubRegister;
    pubBuffer[1] = ubValue;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        i2c2_write(AD9544_I2C_ADDR, pubBuffer, 2, I2C_STOP);
    }
}
static void ad9544_rmw_register(uint8_t ubRegister, uint8_t ubMask, uint8_t ubValue)
{
    ad9544_write_register(ubRegister, (ad9544_read_register(ubRegister) & ubMask) | ubValue);
}

uint8_t ad9544_init()
{
    if(!i2c2_write(AD9544_I2C_ADDR, NULL, 0, I2C_STOP)) // Check ACK from the expected address
        return 0;

    PWR_1V8_PLL_ENABLE();

    PLL_RESET();
    delay_ms(50);
    PLL_UNRESET();

    // TODO: Work

    return 1;
}
void ad9544_isr()
{
    uint8_t ubFlags = 0;
}
