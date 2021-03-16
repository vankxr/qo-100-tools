#include <em_device.h>
#include <stdlib.h>
#include <math.h>
#include "debug_macros.h"
#include "utils.h"
#include "nvic.h"
#include "atomic.h"
#include "systick.h"
#include "rmu.h"
#include "emu.h"
#include "cmu.h"
#include "gpio.h"
#include "msc.h"
#include "rtcc.h"
#include "adc.h"
#include "crc.h"
#include "usart.h"
#include "i2c.h"
#include "si5351.h"
#include "mcp4728.h"
#include "wdog.h"

// Structs

// Helper macros
#define I2C_SLAVE_ADDRESS                       0x3C
#define I2C_SLAVE_REGISTER_COUNT                256
#define I2C_SLAVE_REGISTER(t, a)                (*(t *)&ubI2CRegister[(a)])
#define I2C_SLAVE_REGISTER_WRITE_MASK(t, a)     (*(t *)&ubI2CRegisterWriteMask[(a)])
#define I2C_SLAVE_REGISTER_READ_MASK(t, a)      (*(t *)&ubI2CRegisterReadMask[(a)])
#define I2C_SLAVE_REGISTER_VIN_VOLTAGE          0xC0 // 32-bit
#define I2C_SLAVE_REGISTER_5V0_VOLTAGE          0xC4 // 32-bit
#define I2C_SLAVE_REGISTER_AVDD_VOLTAGE         0xD0 // 32-bit
#define I2C_SLAVE_REGISTER_DVDD_VOLTAGE         0xD4 // 32-bit
#define I2C_SLAVE_REGISTER_IOVDD_VOLTAGE        0xD8 // 32-bit
#define I2C_SLAVE_REGISTER_CORE_VOLTAGE         0xDC // 32-bit
#define I2C_SLAVE_REGISTER_EMU_TEMP             0xE0 // 32-bit
#define I2C_SLAVE_REGISTER_ADC_TEMP             0xE4 // 32-bit
#define I2C_SLAVE_REGISTER_BIAS_REG_TEMP        0xE8 // 32-bit
#define I2C_SLAVE_REGISTER_SW_VERSION           0xF4 // 16-bit
#define I2C_SLAVE_REGISTER_DEV_UIDL             0xF8 // 32-bit
#define I2C_SLAVE_REGISTER_DEV_UIDH             0xFC // 32-bit

#define LNB1_INDEX  0
#define LNB2_INDEX  1

// Forward declarations
static void reset() __attribute__((noreturn));
static void sleep();

static uint32_t get_free_ram();

static void get_device_name(char *pszDeviceName, uint32_t ulDeviceNameSize);
static uint16_t get_device_revision();

static void wdog_warning_isr();

static void i2c_slave_register_init();
static uint8_t i2c_slave_addr_isr(uint8_t ubRnW);
static uint8_t i2c_slave_tx_data_isr();
static uint8_t i2c_slave_rx_data_isr(uint8_t ubData);

static void lnb_ref_clk_synth_init();
static void lnb_set_ref_frequency(uint8_t ubChannel, uint32_t ulFrequency);
static uint32_t lnb_get_ref_frequency(uint8_t ubChannel);

static void lnb_set_bias_voltage(uint8_t ubChannel, float fVoltage);
static float lnb_get_bias_voltage(uint8_t ubChannel);

// Variables
volatile uint8_t ubI2CRegister[I2C_SLAVE_REGISTER_COUNT];
volatile uint8_t ubI2CRegisterWriteMask[I2C_SLAVE_REGISTER_COUNT];
volatile uint8_t ubI2CRegisterReadMask[I2C_SLAVE_REGISTER_COUNT];
volatile uint8_t ubI2CRegisterPointer = 0x00;
volatile uint8_t ubI2CByteCount = 0;

// ISRs

// Functions
void reset()
{
    SCB->AIRCR = 0x05FA0000 | _VAL2FLD(SCB_AIRCR_SYSRESETREQ, 1);

    while(1);
}
void sleep()
{
    rtcc_set_alarm(rtcc_get_time() + 5);

    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk; // Configure Deep Sleep (EM2/3)

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        IRQ_CLEAR(RTCC_IRQn);

        __DMB(); // Wait for all memory transactions to finish before memory access
        __DSB(); // Wait for all memory transactions to finish before executing instructions
        __ISB(); // Wait for all memory transactions to finish before fetching instructions
        __SEV(); // Set the event flag to ensure the next WFE will be a NOP
        __WFE(); // NOP and clear the event flag
        __WFE(); // Wait for event
        __NOP(); // Prevent debugger crashes

        cmu_init();
        cmu_update_clocks();
    }
}

uint32_t get_free_ram()
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        extern void *_sbrk(int);

        void *pCurrentHeap = _sbrk(1);

        if(!pCurrentHeap)
            return 0;

        uint32_t ulFreeRAM = (uint32_t)__get_MSP() - (uint32_t)pCurrentHeap;

        _sbrk(-1);

        return ulFreeRAM;
    }
}

void get_device_name(char *pszDeviceName, uint32_t ulDeviceNameSize)
{
    uint8_t ubFamily = (DEVINFO->PART & _DEVINFO_PART_DEVICE_FAMILY_MASK) >> _DEVINFO_PART_DEVICE_FAMILY_SHIFT;
    const char* szFamily = "?";

    switch(ubFamily)
    {
        case 0x10: szFamily = "EFR32MG1P";  break;
        case 0x11: szFamily = "EFR32MG1B";  break;
        case 0x12: szFamily = "EFR32MG1V";  break;
        case 0x13: szFamily = "EFR32BG1P";  break;
        case 0x14: szFamily = "EFR32BG1B";  break;
        case 0x15: szFamily = "EFR32BG1V";  break;
        case 0x19: szFamily = "EFR32FG1P";  break;
        case 0x1A: szFamily = "EFR32FG1B";  break;
        case 0x1B: szFamily = "EFR32FG1V";  break;
        case 0x1C: szFamily = "EFR32MG12P"; break;
        case 0x1D: szFamily = "EFR32MG12B"; break;
        case 0x1E: szFamily = "EFR32MG12V"; break;
        case 0x1F: szFamily = "EFR32BG12P"; break;
        case 0x20: szFamily = "EFR32BG12B"; break;
        case 0x21: szFamily = "EFR32BG12V"; break;
        case 0x25: szFamily = "EFR32FG12P"; break;
        case 0x26: szFamily = "EFR32FG12B"; break;
        case 0x27: szFamily = "EFR32FG12V"; break;
        case 0x28: szFamily = "EFR32MG13P"; break;
        case 0x29: szFamily = "EFR32MG13B"; break;
        case 0x2A: szFamily = "EFR32MG13V"; break;
        case 0x2B: szFamily = "EFR32BG13P"; break;
        case 0x2C: szFamily = "EFR32BG13B"; break;
        case 0x2D: szFamily = "EFR32BG13V"; break;
        case 0x2E: szFamily = "EFR32ZG13P"; break;
        case 0x31: szFamily = "EFR32FG13P"; break;
        case 0x32: szFamily = "EFR32FG13B"; break;
        case 0x33: szFamily = "EFR32FG13V"; break;
        case 0x34: szFamily = "EFR32MG14P"; break;
        case 0x35: szFamily = "EFR32MG14B"; break;
        case 0x36: szFamily = "EFR32MG14V"; break;
        case 0x37: szFamily = "EFR32BG14P"; break;
        case 0x38: szFamily = "EFR32BG14B"; break;
        case 0x39: szFamily = "EFR32BG14V"; break;
        case 0x3A: szFamily = "EFR32ZG14P"; break;
        case 0x3D: szFamily = "EFR32FG14P"; break;
        case 0x3E: szFamily = "EFR32FG14B"; break;
        case 0x3F: szFamily = "EFR32FG14V"; break;
        case 0x47: szFamily = "EFM32G";     break;
        case 0x48: szFamily = "EFM32GG";    break;
        case 0x49: szFamily = "EFM32TG";    break;
        case 0x4A: szFamily = "EFM32LG";    break;
        case 0x4B: szFamily = "EFM32WG";    break;
        case 0x4C: szFamily = "EFM32ZG";    break;
        case 0x4D: szFamily = "EFM32HG";    break;
        case 0x51: szFamily = "EFM32PG1B";  break;
        case 0x53: szFamily = "EFM32JG1B";  break;
        case 0x55: szFamily = "EFM32PG12B"; break;
        case 0x57: szFamily = "EFM32JG12B"; break;
        case 0x64: szFamily = "EFM32GG11B"; break;
        case 0x67: szFamily = "EFM32TG11B"; break;
        case 0x6A: szFamily = "EFM32GG12B"; break;
        case 0x78: szFamily = "EZR32LG";    break;
        case 0x79: szFamily = "EZR32WG";    break;
        case 0x7A: szFamily = "EZR32HG";    break;
    }

    uint8_t ubPackage = (DEVINFO->MEMINFO & _DEVINFO_MEMINFO_PKGTYPE_MASK) >> _DEVINFO_MEMINFO_PKGTYPE_SHIFT;
    char cPackage = '?';

    if(ubPackage == 74)
        cPackage = '?';
    else if(ubPackage == 76)
        cPackage = 'L';
    else if(ubPackage == 77)
        cPackage = 'M';
    else if(ubPackage == 81)
        cPackage = 'Q';

    uint8_t ubTempGrade = (DEVINFO->MEMINFO & _DEVINFO_MEMINFO_TEMPGRADE_MASK) >> _DEVINFO_MEMINFO_TEMPGRADE_SHIFT;
    char cTempGrade = '?';

    if(ubTempGrade == 0)
        cTempGrade = 'G';
    else if(ubTempGrade == 1)
        cTempGrade = 'I';
    else if(ubTempGrade == 2)
        cTempGrade = '?';
    else if(ubTempGrade == 3)
        cTempGrade = '?';

    uint16_t usPartNumber = (DEVINFO->PART & _DEVINFO_PART_DEVICE_NUMBER_MASK) >> _DEVINFO_PART_DEVICE_NUMBER_SHIFT;
    uint8_t ubPinCount = (DEVINFO->MEMINFO & _DEVINFO_MEMINFO_PINCOUNT_MASK) >> _DEVINFO_MEMINFO_PINCOUNT_SHIFT;

    snprintf(pszDeviceName, ulDeviceNameSize, "%s%huF%hu%c%c%hhu", szFamily, usPartNumber, FLASH_SIZE >> 10, cTempGrade, cPackage, ubPinCount);
}
uint16_t get_device_revision()
{
    uint16_t usRevision;

    /* CHIP MAJOR bit [3:0]. */
    usRevision = ((ROMTABLE->PID0 & _ROMTABLE_PID0_REVMAJOR_MASK) >> _ROMTABLE_PID0_REVMAJOR_SHIFT) << 8;
    /* CHIP MINOR bit [7:4]. */
    usRevision |= ((ROMTABLE->PID2 & _ROMTABLE_PID2_REVMINORMSB_MASK) >> _ROMTABLE_PID2_REVMINORMSB_SHIFT) << 4;
    /* CHIP MINOR bit [3:0]. */
    usRevision |= (ROMTABLE->PID3 & _ROMTABLE_PID3_REVMINORLSB_MASK) >> _ROMTABLE_PID3_REVMINORLSB_SHIFT;

    return usRevision;
}

void wdog_warning_isr()
{
    DBGPRINTLN_CTX("Watchdog warning!");
}

void i2c_slave_register_init()
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        I2C_SLAVE_REGISTER              (float,    I2C_SLAVE_REGISTER_VIN_VOLTAGE)      = -1.f;
        I2C_SLAVE_REGISTER_WRITE_MASK   (uint32_t, I2C_SLAVE_REGISTER_VIN_VOLTAGE)      = 0x00000000;
        I2C_SLAVE_REGISTER_READ_MASK    (uint32_t, I2C_SLAVE_REGISTER_VIN_VOLTAGE)      = 0xFFFFFFFF;
        I2C_SLAVE_REGISTER              (float,    I2C_SLAVE_REGISTER_5V0_VOLTAGE)      = -1.f;
        I2C_SLAVE_REGISTER_WRITE_MASK   (uint32_t, I2C_SLAVE_REGISTER_5V0_VOLTAGE)      = 0x00000000;
        I2C_SLAVE_REGISTER_READ_MASK    (uint32_t, I2C_SLAVE_REGISTER_5V0_VOLTAGE)      = 0xFFFFFFFF;
        I2C_SLAVE_REGISTER              (float,    I2C_SLAVE_REGISTER_AVDD_VOLTAGE)     = -1.f;
        I2C_SLAVE_REGISTER_WRITE_MASK   (uint32_t, I2C_SLAVE_REGISTER_AVDD_VOLTAGE)     = 0x00000000;
        I2C_SLAVE_REGISTER_READ_MASK    (uint32_t, I2C_SLAVE_REGISTER_AVDD_VOLTAGE)     = 0xFFFFFFFF;
        I2C_SLAVE_REGISTER              (float,    I2C_SLAVE_REGISTER_DVDD_VOLTAGE)     = -1.f;
        I2C_SLAVE_REGISTER_WRITE_MASK   (uint32_t, I2C_SLAVE_REGISTER_DVDD_VOLTAGE)     = 0x00000000;
        I2C_SLAVE_REGISTER_READ_MASK    (uint32_t, I2C_SLAVE_REGISTER_DVDD_VOLTAGE)     = 0xFFFFFFFF;
        I2C_SLAVE_REGISTER              (float,    I2C_SLAVE_REGISTER_IOVDD_VOLTAGE)    = -1.f;
        I2C_SLAVE_REGISTER_WRITE_MASK   (uint32_t, I2C_SLAVE_REGISTER_IOVDD_VOLTAGE)    = 0x00000000;
        I2C_SLAVE_REGISTER_READ_MASK    (uint32_t, I2C_SLAVE_REGISTER_IOVDD_VOLTAGE)    = 0xFFFFFFFF;
        I2C_SLAVE_REGISTER              (float,    I2C_SLAVE_REGISTER_CORE_VOLTAGE)     = -1.f;
        I2C_SLAVE_REGISTER_WRITE_MASK   (uint32_t, I2C_SLAVE_REGISTER_CORE_VOLTAGE)     = 0x00000000;
        I2C_SLAVE_REGISTER_READ_MASK    (uint32_t, I2C_SLAVE_REGISTER_CORE_VOLTAGE)     = 0xFFFFFFFF;
        I2C_SLAVE_REGISTER              (float,    I2C_SLAVE_REGISTER_EMU_TEMP)         = -1.f;
        I2C_SLAVE_REGISTER_WRITE_MASK   (uint32_t, I2C_SLAVE_REGISTER_EMU_TEMP)         = 0x00000000;
        I2C_SLAVE_REGISTER_READ_MASK    (uint32_t, I2C_SLAVE_REGISTER_EMU_TEMP)         = 0xFFFFFFFF;
        I2C_SLAVE_REGISTER              (float,    I2C_SLAVE_REGISTER_ADC_TEMP)         = -1.f;
        I2C_SLAVE_REGISTER_WRITE_MASK   (uint32_t, I2C_SLAVE_REGISTER_ADC_TEMP)         = 0x00000000;
        I2C_SLAVE_REGISTER_READ_MASK    (uint32_t, I2C_SLAVE_REGISTER_ADC_TEMP)         = 0xFFFFFFFF;
        I2C_SLAVE_REGISTER              (float,    I2C_SLAVE_REGISTER_BIAS_REG_TEMP)    = -1.f;
        I2C_SLAVE_REGISTER_WRITE_MASK   (uint32_t, I2C_SLAVE_REGISTER_BIAS_REG_TEMP)    = 0x00000000;
        I2C_SLAVE_REGISTER_READ_MASK    (uint32_t, I2C_SLAVE_REGISTER_BIAS_REG_TEMP)    = 0xFFFFFFFF;
        I2C_SLAVE_REGISTER              (uint16_t, I2C_SLAVE_REGISTER_SW_VERSION)       = BUILD_VERSION;
        I2C_SLAVE_REGISTER_WRITE_MASK   (uint16_t, I2C_SLAVE_REGISTER_SW_VERSION)       = 0x0000;
        I2C_SLAVE_REGISTER_READ_MASK    (uint16_t, I2C_SLAVE_REGISTER_SW_VERSION)       = 0xFFFF;
        I2C_SLAVE_REGISTER              (uint32_t, I2C_SLAVE_REGISTER_DEV_UIDL)         = DEVINFO->UNIQUEL;
        I2C_SLAVE_REGISTER_WRITE_MASK   (uint32_t, I2C_SLAVE_REGISTER_DEV_UIDL)         = 0x00000000;
        I2C_SLAVE_REGISTER_READ_MASK    (uint32_t, I2C_SLAVE_REGISTER_DEV_UIDL)         = 0xFFFFFFFF;
        I2C_SLAVE_REGISTER              (uint32_t, I2C_SLAVE_REGISTER_DEV_UIDH)         = DEVINFO->UNIQUEH;
        I2C_SLAVE_REGISTER_WRITE_MASK   (uint32_t, I2C_SLAVE_REGISTER_DEV_UIDH)         = 0x00000000;
        I2C_SLAVE_REGISTER_READ_MASK    (uint32_t, I2C_SLAVE_REGISTER_DEV_UIDH)         = 0xFFFFFFFF;
    }
}
uint8_t i2c_slave_addr_isr(uint8_t ubAddress)
{
    ubI2CByteCount = 0;

    // Hardware address comparator already verifies if the address matches
    // This is only called if address is valid
    // All we need to do is ACK

    return 1; // ACK
}
uint8_t i2c_slave_tx_data_isr()
{
    ubI2CByteCount++;

    uint8_t ubData = ubI2CRegister[ubI2CRegisterPointer] & ubI2CRegisterReadMask[ubI2CRegisterPointer];
    ubI2CRegisterPointer++;

    return ubData;
}
uint8_t i2c_slave_rx_data_isr(uint8_t ubData)
{
    ubI2CByteCount++;

    if(ubI2CByteCount == 1)
    {
        ubI2CRegisterPointer = ubData;

        return 1; // ACK
    }

    ubI2CRegister[ubI2CRegisterPointer] = (ubI2CRegister[ubI2CRegisterPointer] & ~ubI2CRegisterWriteMask[ubI2CRegisterPointer]) | (ubData & ubI2CRegisterWriteMask[ubI2CRegisterPointer]);
    ubI2CRegisterPointer++;

    return 1; // ACK
}

void lnb_ref_clk_synth_init()
{
    si5351_xtal_config(26000000, 10); // fPFD = XTAL, CLoad = 10 pF
    //si5351_clkin_config(50000000, 2); // fPFD = CLKIN / 2

    DBGPRINTLN_CTX("XTAL Clock: %.3f MHz", (float)SI5351_CLKIN_FREQ / 1000000);
    DBGPRINTLN_CTX("CLKIN Clock: %.3f MHz", (float)SI5351_CLKIN_FREQ / 1000000);
    DBGPRINTLN_CTX("CLKIN Divider Clock: %.3f MHz", (float)SI5351_CLKIN_DIV_FREQ / 1000000);

    //// PLLA
    si5351_pll_set_source(SI5351_PLLA, SI5351_PLL_SRC_XTAL);
    si5351_pll_set_freq(SI5351_PLLA, 800000000);

    DBGPRINTLN_CTX("PLLA Source Clock: %.3f MHz", (float)SI5351_PLL_SRC_FREQ[SI5351_PLLA] / 1000000);
    DBGPRINTLN_CTX("PLLA VCO Clock: %.3f MHz", (float)SI5351_PLL_FREQ[SI5351_PLLA] / 1000000);

    //// PLLB
    /*
    si5351_pll_set_source(SI5351_PLLB, SI5351_PLL_SRC_XTAL);
    si5351_pll_set_freq(SI5351_PLLB, 840000000);

    DBGPRINTLN_CTX("PLLB Source Clock: %.3f MHz", (float)SI5351_PLL_SRC_FREQ[SI5351_PLLB] / 1000000);
    DBGPRINTLN_CTX("PLLB VCO Clock: %.3f MHz", (float)SI5351_PLL_FREQ[SI5351_PLLB] / 1000000);
    */

    //// LNB #1 Reference frequency
    si5351_multisynth_set_source(SI5351_LNB1_REF, SI5351_MS_SRC_PLLA);
    si5351_multisynth_set_freq(SI5351_LNB1_REF, 25000000);
    si5351_multisynth_set_phase_offset(SI5351_LNB1_REF, 0.f);

    DBGPRINTLN_CTX("MS%hhu Source Clock: %.3f MHz", SI5351_LNB1_REF, (float)SI5351_MS_SRC_FREQ[SI5351_LNB1_REF] / 1000000);
    DBGPRINTLN_CTX("MS%hhu Clock: %.3f MHz", SI5351_LNB1_REF, (float)SI5351_MS_FREQ[SI5351_LNB1_REF] / 1000000);
    DBGPRINTLN_CTX("MS%hhu Phase offset: %.1f deg", SI5351_LNB1_REF, si5351_multisynth_get_phase_offset(SI5351_LNB1_REF));

    si5351_clock_set_disable_state(SI5351_LNB1_REF, SI5351_REG_CLKm_n_DIS_DISn_HIZ); // Disable in High-Z mode
    si5351_clock_set_drive_current(SI5351_LNB1_REF, 8); // 8 mA
    si5351_clock_set_invert(SI5351_LNB1_REF, 0); // Not inverted
    si5351_clock_set_source(SI5351_LNB1_REF, SI5351_CLK_SRC_MSn); // Corresponding multisynth as source
    si5351_clock_set_output_divider(SI5351_LNB1_REF, 1); // Divide by 1 at the output

    DBGPRINTLN_CTX("CLK%hhu Clock: %.3f MHz", SI5351_LNB1_REF, (float)SI5351_CLK_FREQ[SI5351_LNB1_REF] / 1000000);

    si5351_clock_power_up(SI5351_LNB1_REF); // Power the output stage up
    //si5351_clock_enable(SI5351_LNB1_REF); // Software enable the clock output

    //// LNB #2 Reference frequency
    si5351_multisynth_set_source(SI5351_LNB2_REF, SI5351_MS_SRC_PLLA);
    si5351_multisynth_set_freq(SI5351_LNB2_REF, 25000000);
    si5351_multisynth_set_phase_offset(SI5351_LNB2_REF, 0.f);

    DBGPRINTLN_CTX("MS%hhu Source Clock: %.3f MHz", SI5351_LNB2_REF, (float)SI5351_MS_SRC_FREQ[SI5351_LNB2_REF] / 1000000);
    DBGPRINTLN_CTX("MS%hhu Clock: %.3f MHz", SI5351_LNB2_REF, (float)SI5351_MS_FREQ[SI5351_LNB2_REF] / 1000000);
    DBGPRINTLN_CTX("MS%hhu Phase offset: %.1f deg", SI5351_LNB2_REF, si5351_multisynth_get_phase_offset(SI5351_LNB2_REF));

    si5351_clock_set_disable_state(SI5351_LNB2_REF, SI5351_REG_CLKm_n_DIS_DISn_HIZ); // Disable in High-Z mode
    si5351_clock_set_drive_current(SI5351_LNB2_REF, 8); // 8 mA
    si5351_clock_set_invert(SI5351_LNB2_REF, 0); // Not inverted
    si5351_clock_set_source(SI5351_LNB2_REF, SI5351_CLK_SRC_MSn); // Corresponding multisynth as source
    si5351_clock_set_output_divider(SI5351_LNB2_REF, 1); // Divide by 1 at the output

    DBGPRINTLN_CTX("CLK%hhu Clock: %.3f MHz", SI5351_LNB2_REF, (float)SI5351_CLK_FREQ[SI5351_LNB2_REF] / 1000000);

    si5351_clock_power_up(SI5351_LNB2_REF); // Power the output stage up
    //si5351_clock_enable(SI5351_LNB2_REF); // Software enable the clock output

    //// External reference output frequency
    si5351_multisynth_set_source(SI5351_EXT_REF, SI5351_MS_SRC_PLLA);
    si5351_multisynth_set_freq(SI5351_EXT_REF, 10000000);
    si5351_multisynth_set_phase_offset(SI5351_EXT_REF, 0.f);

    DBGPRINTLN_CTX("MS%hhu Source Clock: %.3f MHz", SI5351_EXT_REF, (float)SI5351_MS_SRC_FREQ[SI5351_EXT_REF] / 1000000);
    DBGPRINTLN_CTX("MS%hhu Clock: %.3f MHz", SI5351_EXT_REF, (float)SI5351_MS_FREQ[SI5351_EXT_REF] / 1000000);
    DBGPRINTLN_CTX("MS%hhu Phase offset: %.1f deg", SI5351_EXT_REF, si5351_multisynth_get_phase_offset(SI5351_EXT_REF));

    si5351_clock_set_disable_state(SI5351_EXT_REF, SI5351_REG_CLKm_n_DIS_DISn_HIZ); // Disable in High-Z mode
    si5351_clock_set_drive_current(SI5351_EXT_REF, 8); // 8 mA
    si5351_clock_set_invert(SI5351_EXT_REF, 0); // Not inverted
    si5351_clock_set_source(SI5351_EXT_REF, SI5351_CLK_SRC_MSn); // Corresponding multisynth as source
    si5351_clock_set_output_divider(SI5351_EXT_REF, 1); // Divide by 1 at the output

    DBGPRINTLN_CTX("CLK%hhu Clock: %.3f MHz", SI5351_EXT_REF, (float)SI5351_CLK_FREQ[SI5351_EXT_REF] / 1000000);

    si5351_clock_power_up(SI5351_EXT_REF); // Power the output stage up
    si5351_clock_enable(SI5351_EXT_REF); // Software enable the clock output

    //// Global output enable
    REF_SYNTH_OUT_EN();
}
void lnb_set_ref_frequency(uint8_t ubChannel, uint32_t ulFrequency)
{
    if(ubChannel > 1)
        return;

    switch(ubChannel)
    {
        case LNB1_INDEX:
        {
            if(!ulFrequency)
            {
                si5351_clock_disable(SI5351_LNB1_REF);

                break;
            }

            si5351_multisynth_set_freq(SI5351_LNB1_REF, ulFrequency);
            si5351_clock_enable(SI5351_LNB1_REF);
        }
        break;
        case LNB2_INDEX:
        {
            if(!ulFrequency)
            {
                si5351_clock_disable(SI5351_LNB2_REF);

                break;
            }

            si5351_multisynth_set_freq(SI5351_LNB2_REF, ulFrequency);
            si5351_clock_enable(SI5351_LNB2_REF);
        }
        break;
    }
}
uint32_t lnb_get_ref_frequency(uint8_t ubChannel)
{
    if(ubChannel > 1)
        return 0;

    switch(ubChannel)
    {
        case LNB1_INDEX:
            return SI5351_MS_FREQ[SI5351_LNB1_REF];
        case LNB2_INDEX:
            return SI5351_MS_FREQ[SI5351_LNB2_REF];
    }

    return 0;
}

void lnb_set_bias_voltage(uint8_t ubChannel, float fVoltage)
{
    if(ubChannel > 1)
        return;

    float fDACVoltage = -0.191f * fVoltage + 3948.87f;

    if(fDACVoltage < 0.f || fDACVoltage >= MCP4728_INTERNAL_REF)
        return;

    uint16_t usCode = fDACVoltage / MCP4728_INTERNAL_REF * 4096.f;

    if(usCode > 4095)
        usCode = 4095;

    mcp4728_channel_write(ubChannel, MCP4728_CHAN_VREF_INTERNAL | MCP4728_CHAN_PD_NORMAL | MCP4728_CHAN_GAIN_X1 | (usCode & 0x0FFF));
}
float lnb_get_bias_voltage(uint8_t ubChannel)
{
    if(ubChannel > 3)
        return 0.f;

    uint16_t usCode = mcp4728_channel_read(ubChannel);
    float fDACVoltage = 0.f;

    if(usCode & MCP4728_CHAN_VREF_INTERNAL)
        fDACVoltage = (usCode & 0x0FFF) * (MCP4728_INTERNAL_REF / 4096.f);
    else
        fDACVoltage = (usCode & 0x0FFF) * (MCP4728_EXTERNAL_REF / 4096.f);

    if(usCode & MCP4728_CHAN_GAIN_X2)
        fDACVoltage *= 2.f;

    float fVoltage = -5.2356f * fDACVoltage + 20674.75f;

    return fVoltage;
}

int init()
{
    rmu_init(RMU_CTRL_PINRMODE_FULL, RMU_CTRL_SYSRMODE_EXTENDED, RMU_CTRL_LOCKUPRMODE_EXTENDED, RMU_CTRL_WDOGRMODE_EXTENDED); // Init RMU and set reset modes

    emu_init(1); // Init EMU, ignore DCDC and switch digital power immediatly to DVDD

    cmu_init(); // Init Clocks

    msc_init(); // Init Flash, RAM and caches

    systick_init(); // Init system tick

    wdog_init((8 <<_WDOG_CTRL_PERSEL_SHIFT) | (3 << _WDOG_CTRL_WARNSEL_SHIFT)); // Init the watchdog timer, 2049 ms timeout, 75% warning
    wdog_set_warning_isr(wdog_warning_isr);

    gpio_init(); // Init GPIOs
    ldma_init(); // Init LDMA
    rtcc_init(); // Init RTCC
    crc_init(); // Init CRC calculation unit
    adc_init(); // Init ADCs

    float fAVDDHighThresh, fAVDDLowThresh;
    float fDVDDHighThresh, fDVDDLowThresh;
    float fIOVDDHighThresh, fIOVDDLowThresh;

    emu_vmon_avdd_config(1, 3.1f, &fAVDDLowThresh, 3.22f, &fAVDDHighThresh); // Enable AVDD monitor
    emu_vmon_dvdd_config(1, 3.1f, &fDVDDLowThresh); // Enable DVDD monitor
    emu_vmon_iovdd_config(1, 3.15f, &fIOVDDLowThresh); // Enable IOVDD monitor

    fDVDDHighThresh = fDVDDLowThresh + 0.026f; // Hysteresis from datasheet
    fIOVDDHighThresh = fIOVDDLowThresh + 0.026f; // Hysteresis from datasheet

    usart1_init(1000000, USART_FRAME_STOPBITS_ONE | USART_FRAME_PARITY_NONE | USART_FRAME_DATABITS_EIGHT, -1, 5, -1, -1);

    i2c0_init(I2C_SLAVE_ADDRESS, 1, 1);
    i2c0_set_slave_addr_isr(i2c_slave_addr_isr);
    i2c0_set_slave_tx_data_isr(i2c_slave_tx_data_isr);
    i2c0_set_slave_rx_data_isr(i2c_slave_rx_data_isr);
    i2c1_init(I2C_FAST, 3, 3);

    char szDeviceName[32];

    get_device_name(szDeviceName, 32);

    DBGPRINTLN_CTX("IcyRadio LNB Controller v%lu (%s %s)!", BUILD_VERSION, __DATE__, __TIME__);
    DBGPRINTLN_CTX("Device: %s", szDeviceName);
    DBGPRINTLN_CTX("Device Revision: 0x%04X", get_device_revision());
    DBGPRINTLN_CTX("Calibration temperature: %hhu C", (DEVINFO->CAL & _DEVINFO_CAL_TEMP_MASK) >> _DEVINFO_CAL_TEMP_SHIFT);
    DBGPRINTLN_CTX("Flash Size: %hu kB", FLASH_SIZE >> 10);
    DBGPRINTLN_CTX("RAM Size: %hu kB", SRAM_SIZE >> 10);
    DBGPRINTLN_CTX("Free RAM: %lu B", get_free_ram());
    DBGPRINTLN_CTX("Unique ID: %08X-%08X", DEVINFO->UNIQUEH, DEVINFO->UNIQUEL);

    DBGPRINTLN_CTX("RMU - Reset cause: %hhu", rmu_get_reset_reason());
    DBGPRINTLN_CTX("RMU - Reset state: %hhu", rmu_get_reset_state());

    rmu_clear_reset_reason();

    DBGPRINTLN_CTX("EMU - AVDD Fall Threshold: %.2f mV!", fAVDDLowThresh * 1000);
    DBGPRINTLN_CTX("EMU - AVDD Rise Threshold: %.2f mV!", fAVDDHighThresh * 1000);
    DBGPRINTLN_CTX("EMU - AVDD Voltage: %.2f mV", adc_get_avdd());
    DBGPRINTLN_CTX("EMU - AVDD Status: %s", g_ubAVDDLow ? "LOW" : "OK");
    DBGPRINTLN_CTX("EMU - DVDD Fall Threshold: %.2f mV!", fDVDDLowThresh * 1000);
    DBGPRINTLN_CTX("EMU - DVDD Rise Threshold: %.2f mV!", fDVDDHighThresh * 1000);
    DBGPRINTLN_CTX("EMU - DVDD Voltage: %.2f mV", adc_get_dvdd());
    DBGPRINTLN_CTX("EMU - DVDD Status: %s", g_ubDVDDLow ? "LOW" : "OK");
    DBGPRINTLN_CTX("EMU - IOVDD Fall Threshold: %.2f mV!", fIOVDDLowThresh * 1000);
    DBGPRINTLN_CTX("EMU - IOVDD Rise Threshold: %.2f mV!", fIOVDDHighThresh * 1000);
    DBGPRINTLN_CTX("EMU - IOVDD Voltage: %.2f mV", adc_get_iovdd());
    DBGPRINTLN_CTX("EMU - IOVDD Status: %s", g_ubIOVDDLow ? "LOW" : "OK");
    DBGPRINTLN_CTX("EMU - Core Voltage: %.2f mV", adc_get_corevdd());
    DBGPRINTLN_CTX("EMU - VIN Voltage: %.2f mV", adc_get_vin());
    DBGPRINTLN_CTX("EMU - 5V0 Voltage: %.2f mV", adc_get_5v0());

    DBGPRINTLN_CTX("CMU - HFXO Oscillator: %.3f MHz", (float)HFXO_OSC_FREQ / 1000000);
    DBGPRINTLN_CTX("CMU - HFRCO Oscillator: %.3f MHz", (float)HFRCO_OSC_FREQ / 1000000);
    DBGPRINTLN_CTX("CMU - AUXHFRCO Oscillator: %.3f MHz", (float)AUXHFRCO_OSC_FREQ / 1000000);
    DBGPRINTLN_CTX("CMU - LFXO Oscillator: %.3f kHz", (float)LFXO_OSC_FREQ / 1000);
    DBGPRINTLN_CTX("CMU - LFRCO Oscillator: %.3f kHz", (float)LFRCO_OSC_FREQ / 1000);
    DBGPRINTLN_CTX("CMU - ULFRCO Oscillator: %.3f kHz", (float)ULFRCO_OSC_FREQ / 1000);
    DBGPRINTLN_CTX("CMU - HFSRC Clock: %.3f MHz", (float)HFSRC_CLOCK_FREQ / 1000000);
    DBGPRINTLN_CTX("CMU - HF Clock: %.3f MHz", (float)HF_CLOCK_FREQ / 1000000);
    DBGPRINTLN_CTX("CMU - HFBUS Clock: %.3f MHz", (float)HFBUS_CLOCK_FREQ / 1000000);
    DBGPRINTLN_CTX("CMU - HFCORE Clock: %.3f MHz", (float)HFCORE_CLOCK_FREQ / 1000000);
    DBGPRINTLN_CTX("CMU - HFEXP Clock: %.3f MHz", (float)HFEXP_CLOCK_FREQ / 1000000);
    DBGPRINTLN_CTX("CMU - HFPER Clock: %.3f MHz", (float)HFPER_CLOCK_FREQ / 1000000);
    DBGPRINTLN_CTX("CMU - HFPERB Clock: %.3f MHz", (float)HFPERB_CLOCK_FREQ / 1000000);
    DBGPRINTLN_CTX("CMU - HFPERC Clock: %.3f MHz", (float)HFPERC_CLOCK_FREQ / 1000000);
    DBGPRINTLN_CTX("CMU - HFLE Clock: %.3f MHz", (float)HFLE_CLOCK_FREQ / 1000000);
    DBGPRINTLN_CTX("CMU - ADC0 Clock: %.3f MHz", (float)ADC0_CLOCK_FREQ / 1000000);
    DBGPRINTLN_CTX("CMU - DBG Clock: %.3f MHz", (float)DBG_CLOCK_FREQ / 1000000);
    DBGPRINTLN_CTX("CMU - AUX Clock: %.3f MHz", (float)AUX_CLOCK_FREQ / 1000000);
    DBGPRINTLN_CTX("CMU - LFA Clock: %.3f kHz", (float)LFA_CLOCK_FREQ / 1000);
    DBGPRINTLN_CTX("CMU - LESENSE Clock: %.3f kHz", (float)LESENSE_CLOCK_FREQ / 1000);
    DBGPRINTLN_CTX("CMU - LETIMER0 Clock: %.3f kHz", (float)LETIMER0_CLOCK_FREQ / 1000);
    DBGPRINTLN_CTX("CMU - LFB Clock: %.3f kHz", (float)LFB_CLOCK_FREQ / 1000);
    DBGPRINTLN_CTX("CMU - LEUART0 Clock: %.3f kHz", (float)LEUART0_CLOCK_FREQ / 1000);
    DBGPRINTLN_CTX("CMU - SYSTICK Clock: %.3f kHz", (float)SYSTICK_CLOCK_FREQ / 1000);
    DBGPRINTLN_CTX("CMU - CSEN Clock: %.3f kHz", (float)CSEN_CLOCK_FREQ / 1000);
    DBGPRINTLN_CTX("CMU - LFE Clock: %.3f kHz", (float)LFE_CLOCK_FREQ / 1000);
    DBGPRINTLN_CTX("CMU - RTCC Clock: %.3f kHz", (float)RTCC_CLOCK_FREQ / 1000);

    DBGPRINTLN_CTX("WDOG - Timeout period: %.3f ms", wdog_get_timeout_period());
    DBGPRINTLN_CTX("WDOG - Warning period: %.3f ms", wdog_get_warning_period());

    DBGPRINTLN_CTX("Scanning I2C bus 1...");

    for(uint8_t a = 0x08; a < 0x78; a++)
        if(i2c1_write(a, NULL, 0, I2C_STOP))
            DBGPRINTLN_CTX("  Address 0x%02X ACKed!", a);

    if(si5351_init())
        DBGPRINTLN_CTX("SI5351 init OK!");
    else
        DBGPRINTLN_CTX("SI5351 init NOK!");

    if(mcp4728_init())
        DBGPRINTLN_CTX("MCP4728 init OK!");
    else
        DBGPRINTLN_CTX("MCP4728 init NOK!");

    return 0;
}
int main()
{
    // I2C Slave Register block
    i2c_slave_register_init();

    // LNB Reference synthesizer
    lnb_ref_clk_synth_init();

    //lnb_set_bias_voltage(LNB1_INDEX, 13500);
    //VBIAS1_ENABLE();

    while(1)
    {
        wdog_feed();

        static uint64_t ullLastHeartBeat = 0;
        static uint64_t ullLastTelemetryUpdate = 0;

        if(g_ullSystemTick - ullLastHeartBeat > 2000)
        {
            ullLastHeartBeat = g_ullSystemTick;

            LED_TOGGLE();

            if(LED_STATUS())
                ullLastHeartBeat -= 1900;
        }

        if(g_ullSystemTick - ullLastTelemetryUpdate > 5000)
        {
            ullLastTelemetryUpdate = g_ullSystemTick;

            // System Temperatures
            float fADCTemp = adc_get_temperature();
            float fEMUTemp = emu_get_temperature();
            float fBiasRegTemp = adc_get_bias_reg_temperature();

            ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
            {
                I2C_SLAVE_REGISTER(float, I2C_SLAVE_REGISTER_ADC_TEMP) = fADCTemp;
                I2C_SLAVE_REGISTER(float, I2C_SLAVE_REGISTER_EMU_TEMP) = fEMUTemp;
                I2C_SLAVE_REGISTER(float, I2C_SLAVE_REGISTER_BIAS_REG_TEMP) = fBiasRegTemp;
            }

            DBGPRINTLN_CTX("----------------------------------");
            DBGPRINTLN_CTX("ADC Temperature: %.2f C", fADCTemp);
            DBGPRINTLN_CTX("EMU Temperature: %.2f C", fEMUTemp);
            DBGPRINTLN_CTX("Bias Regulator Temperature: %.2f C", fBiasRegTemp);

            // System Voltages/Currents
            float fVIN = adc_get_vin();
            float f5V0 = adc_get_5v0();
            float fAVDD = adc_get_avdd();
            float fDVDD = adc_get_dvdd();
            float fIOVDD = adc_get_iovdd();
            float fCoreVDD = adc_get_corevdd();

            ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
            {
                I2C_SLAVE_REGISTER(float, I2C_SLAVE_REGISTER_VIN_VOLTAGE) = fVIN;
                I2C_SLAVE_REGISTER(float, I2C_SLAVE_REGISTER_5V0_VOLTAGE) = f5V0;
                I2C_SLAVE_REGISTER(float, I2C_SLAVE_REGISTER_AVDD_VOLTAGE) = fAVDD;
                I2C_SLAVE_REGISTER(float, I2C_SLAVE_REGISTER_DVDD_VOLTAGE) = fDVDD;
                I2C_SLAVE_REGISTER(float, I2C_SLAVE_REGISTER_IOVDD_VOLTAGE) = fIOVDD;
                I2C_SLAVE_REGISTER(float, I2C_SLAVE_REGISTER_CORE_VOLTAGE) = fCoreVDD;
            }

            DBGPRINTLN_CTX("----------------------------------");
            DBGPRINTLN_CTX("AVDD Voltage: %.2f mV", fAVDD);
            DBGPRINTLN_CTX("DVDD Voltage: %.2f mV", fDVDD);
            DBGPRINTLN_CTX("IOVDD Voltage: %.2f mV", fIOVDD);
            DBGPRINTLN_CTX("Core Voltage: %.2f mV", fCoreVDD);
            DBGPRINTLN_CTX("VIN Voltage: %.2f mV", fVIN);
            DBGPRINTLN_CTX("5V0 Voltage: %.2f mV", f5V0);

            // LNB Bias Voltages
            float fVBiasSet1 = lnb_get_bias_voltage(LNB1_INDEX);
            float fVBias1 = adc_get_vbias1();
            float fVBiasSet2 = lnb_get_bias_voltage(LNB2_INDEX);
            float fVBias2 = adc_get_vbias2();

            DBGPRINTLN_CTX("----------------------------------");
            DBGPRINTLN_CTX("LNB #1 Enable status: %s", VBIAS1_STATUS() ? "ON" : "OFF");
            DBGPRINTLN_CTX("LNB #1 Power status: %s", VBIAS1_POWER_GOOD() ? "POWER GOOD" : "POWER NOT GOOD");
            DBGPRINTLN_CTX("LNB #1 Set Bias Voltage: %.2f mV", fVBiasSet1);
            DBGPRINTLN_CTX("LNB #1 Bias Voltage: %.2f mV", fVBias1);
            DBGPRINTLN_CTX("LNB #2 Enable status: %s", VBIAS2_STATUS() ? "ON" : "OFF");
            DBGPRINTLN_CTX("LNB #2 Power status: %s", VBIAS2_POWER_GOOD() ? "POWER GOOD" : "POWER NOT GOOD");
            DBGPRINTLN_CTX("LNB #2 Set Bias Voltage: %.2f mV", fVBiasSet2);
            DBGPRINTLN_CTX("LNB #2 Bias Voltage: %.2f mV", fVBias2);
        }
    }

    return 0;
}