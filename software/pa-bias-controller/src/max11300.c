#include "max11300.h"

static uint16_t max11300_read_register(uint8_t ubRegister)
{
    uint16_t usValue = 0;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        AFE_SELECT();

        usart0_spi_write_byte(((ubRegister & 0x7F) << 1) | BIT(0), 0);
        usValue |= (uint16_t)usart0_spi_transfer_byte(0x00) << 8;
        usValue |= (uint16_t)usart0_spi_transfer_byte(0x00) << 0;

        AFE_UNSELECT();
    }

    return usValue;
}
static uint32_t max11300_read_register32(uint8_t ubRegister)
{
    uint32_t ulValue = 0;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        AFE_SELECT();

        usart0_spi_write_byte(((ubRegister & 0x7F) << 1) | BIT(0), 0);
        ulValue |= (uint32_t)usart0_spi_transfer_byte(0x00) << 8;
        ulValue |= (uint32_t)usart0_spi_transfer_byte(0x00) << 0;
        ulValue |= (uint32_t)usart0_spi_transfer_byte(0x00) << 24;
        ulValue |= (uint32_t)usart0_spi_transfer_byte(0x00) << 16;

        AFE_UNSELECT();
    }

    return ulValue;
}
static void max11300_write_register(uint8_t ubRegister, uint16_t usValue)
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        AFE_SELECT();

        usart0_spi_write_byte(((ubRegister & 0x7F) << 1) & ~BIT(0), 0);
        usart0_spi_write_byte((usValue >> 8) & 0xFF, 0);
        usart0_spi_write_byte((usValue >> 0) & 0xFF, 1);

        AFE_UNSELECT();
    }
}
static void max11300_rmw_register(uint8_t ubRegister, uint16_t usMask, uint16_t usValue)
{
    max11300_write_register(ubRegister, (max11300_read_register(ubRegister) & usMask) | usValue);
}

uint8_t max11300_init()
{
    if(max11300_read_device_id() != 0x424)
        return 0;

    max11300_reset();

    return 1;
}
void max11300_isr()
{
    uint16_t usFlags = max11300_read_register(MAX11300_REG_INTERRUPT);


}

void max11300_reset()
{
    max11300_rmw_register(MAX11300_REG_DEVICE_CTRL, (uint16_t)~MAX11300_REG_DEVICE_CTRL_RESET, MAX11300_REG_DEVICE_CTRL_RESET);
}

uint16_t max11300_read_device_id()
{
    return max11300_read_register(MAX11300_REG_DEVICE_ID);
}

void max11300_config(uint16_t usConfig)
{
    max11300_rmw_register(MAX11300_REG_DEVICE_CTRL, 0x0F00, usConfig);
}

void max11300_int_temp_config(uint8_t ubEnable, uint16_t usSamples, float fLowThresh, float fHighThresh)
{
    if(!ubEnable)
    {
        max11300_rmw_register(MAX11300_REG_DEVICE_CTRL, (uint16_t)~MAX11300_REG_DEVICE_CTRL_TMPCTL_INT, 0);

        return;
    }

    max11300_rmw_register(MAX11300_REG_TEMP_MON_CONFIG, (uint16_t)~MAX11300_REG_TEMP_MON_CONFIG_TMPINTMONCFG_32_SAMPLES, usSamples & MAX11300_REG_TEMP_MON_CONFIG_TMPINTMONCFG_32_SAMPLES);
    max11300_write_register(MAX11300_REG_INT_TEMP_THRESH_LOW, (uint16_t)(fLowThresh * 8) & 0x0FFF);
    max11300_write_register(MAX11300_REG_INT_TEMP_THRESH_HIGH, (uint16_t)(fHighThresh * 8) & 0x0FFF);
    max11300_rmw_register(MAX11300_REG_DEVICE_CTRL, (uint16_t)~MAX11300_REG_DEVICE_CTRL_TMPCTL_INT, MAX11300_REG_DEVICE_CTRL_TMPCTL_INT);
}
float max11300_int_temp_read()
{
    return (float)max11300_read_register(MAX11300_REG_INT_TEMP) / 8;
}
void max11300_ext1_temp_config(uint8_t ubEnable, uint16_t usSamples, float fLowThresh, float fHighThresh)
{
    if(!ubEnable)
    {
        max11300_rmw_register(MAX11300_REG_DEVICE_CTRL, (uint16_t)~MAX11300_REG_DEVICE_CTRL_TMPCTL_EXT1, 0);

        return;
    }

    max11300_rmw_register(MAX11300_REG_TEMP_MON_CONFIG, (uint16_t)~MAX11300_REG_TEMP_MON_CONFIG_TMPEXT1MONCFG_32_SAMPLES, usSamples & MAX11300_REG_TEMP_MON_CONFIG_TMPEXT1MONCFG_32_SAMPLES);
    max11300_write_register(MAX11300_REG_EXT1_TEMP_THRESH_LOW, (uint16_t)(fLowThresh * 8) & 0x0FFF);
    max11300_write_register(MAX11300_REG_EXT1_TEMP_THRESH_HIGH, (uint16_t)(fHighThresh * 8) & 0x0FFF);
    max11300_rmw_register(MAX11300_REG_DEVICE_CTRL, (uint16_t)~MAX11300_REG_DEVICE_CTRL_TMPCTL_EXT1, MAX11300_REG_DEVICE_CTRL_TMPCTL_EXT1);
}
float max11300_ext1_temp_read()
{
    return (float)max11300_read_register(MAX11300_REG_EXT1_TEMP) / 8;
}
void max11300_ext2_temp_config(uint8_t ubEnable, uint16_t usSamples, float fLowThresh, float fHighThresh)
{
    if(!ubEnable)
    {
        max11300_rmw_register(MAX11300_REG_DEVICE_CTRL, (uint16_t)~MAX11300_REG_DEVICE_CTRL_TMPCTL_EXT2, 0);

        return;
    }

    max11300_rmw_register(MAX11300_REG_TEMP_MON_CONFIG, (uint16_t)~MAX11300_REG_TEMP_MON_CONFIG_TMPEXT2MONCFG_32_SAMPLES, usSamples & MAX11300_REG_TEMP_MON_CONFIG_TMPEXT2MONCFG_32_SAMPLES);
    max11300_write_register(MAX11300_REG_EXT2_TEMP_THRESH_LOW, (uint16_t)(fLowThresh * 8) & 0x0FFF);
    max11300_write_register(MAX11300_REG_EXT2_TEMP_THRESH_HIGH, (uint16_t)(fHighThresh * 8) & 0x0FFF);
    max11300_rmw_register(MAX11300_REG_DEVICE_CTRL, (uint16_t)~MAX11300_REG_DEVICE_CTRL_TMPCTL_EXT2, MAX11300_REG_DEVICE_CTRL_TMPCTL_EXT2);
}
float max11300_ext2_temp_read()
{
    return (float)max11300_read_register(MAX11300_REG_EXT2_TEMP) / 8;
}

void max11300_dac_set_preset(uint8_t ubIndex, uint16_t usData)
{
    if(ubIndex > 1)
        return;

    max11300_write_register(MAX11300_REG_DAC_PRESETn(ubIndex), usData & 0x0FFF);
}
uint16_t max11300_dac_get_preset(uint8_t ubIndex)
{
    if(ubIndex > 1)
        return 0;

    return max11300_read_register(MAX11300_REG_DAC_PRESETn(ubIndex)) & 0x0FFF;
}

void max11300_port_config(uint8_t ubIndex, uint16_t usConfig)
{
    if(ubIndex > 19)
        return;

    max11300_write_register(MAX11300_REG_PORTn_CONFIG(ubIndex), usConfig);
}
void max11300_port_gpo_set(uint8_t ubIndex, uint8_t ubState)
{
    if(ubIndex > 19)
        return;

    max11300_rmw_register(ubIndex < 16 ? MAX11300_REG_GPO_DATA_L : MAX11300_REG_GPO_DATA_H, (uint16_t)~BIT(ubIndex % 16), ubState ? BIT(ubIndex % 16) : 0);
}
uint8_t max11300_port_gpo_get(uint8_t ubIndex)
{
    if(ubIndex > 19)
        return 0;

    return max11300_read_register(ubIndex < 16 ? MAX11300_REG_GPO_DATA_L : MAX11300_REG_GPO_DATA_H) & BIT(ubIndex % 16);
}
uint8_t max11300_port_gpi_get(uint8_t ubIndex)
{
    if(ubIndex > 19)
        return 0;

    return max11300_read_register(ubIndex < 16 ? MAX11300_REG_GPI_DATA_L : MAX11300_REG_GPI_DATA_H) & BIT(ubIndex % 16);
}
void max11300_port_dac_set_data(uint8_t ubIndex, uint16_t usData)
{
    if(ubIndex > 19)
        return;

    max11300_write_register(MAX11300_REG_PORTn_DAC_DATA(ubIndex), usData & 0x0FFF);
}
uint16_t max11300_port_dac_get_data(uint8_t ubIndex)
{
    if(ubIndex > 19)
        return 0;

    return max11300_read_register(MAX11300_REG_PORTn_DAC_DATA(ubIndex)) & 0x0FFF;
}
uint16_t max11300_port_adc_get_data(uint8_t ubIndex)
{
    if(ubIndex > 19)
        return 0;

    return max11300_read_register(MAX11300_REG_PORTn_ADC_DATA(ubIndex)) & 0x0FFF;
}
