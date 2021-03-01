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
#include "wdog.h"

// Structs

// Helper macros
#define I2C_SLAVE_ADDRESS                       0x3A
#define I2C_SLAVE_REGISTER_COUNT                256
#define I2C_SLAVE_REGISTER(t, a)                (*(t *)&ubI2CRegister[(a)])
#define I2C_SLAVE_REGISTER_WRITE_MASK(t, a)     (*(t *)&ubI2CRegisterWriteMask[(a)])
#define I2C_SLAVE_REGISTER_READ_MASK(t, a)      (*(t *)&ubI2CRegisterReadMask[(a)])
#define I2C_SLAVE_REGISTER_RELAY_STATUS         0x00 // 16-bit
#define I2C_SLAVE_REGISTER_RELAY_SET_ON         0x02 // 16-bit
#define I2C_SLAVE_REGISTER_RELAY_SET_OFF        0x04 // 16-bit
#define I2C_SLAVE_REGISTER_RELAY0_DUTY_CYCLE    0x10 // 8-bit
#define I2C_SLAVE_REGISTER_RELAY1_DUTY_CYCLE    0x11 // 8-bit
#define I2C_SLAVE_REGISTER_RELAY2_DUTY_CYCLE    0x12 // 8-bit
#define I2C_SLAVE_REGISTER_RELAY3_DUTY_CYCLE    0x13 // 8-bit
#define I2C_SLAVE_REGISTER_RELAY4_DUTY_CYCLE    0x14 // 8-bit
#define I2C_SLAVE_REGISTER_RELAY5_DUTY_CYCLE    0x15 // 8-bit
#define I2C_SLAVE_REGISTER_RELAY6_DUTY_CYCLE    0x16 // 8-bit
#define I2C_SLAVE_REGISTER_RELAY7_DUTY_CYCLE    0x17 // 8-bit
#define I2C_SLAVE_REGISTER_RELAY8_DUTY_CYCLE    0x18 // 8-bit
#define I2C_SLAVE_REGISTER_RELAY9_DUTY_CYCLE    0x19 // 8-bit
#define I2C_SLAVE_REGISTER_RELAY10_DUTY_CYCLE   0x1A // 8-bit
#define I2C_SLAVE_REGISTER_RELAY11_DUTY_CYCLE   0x1B // 8-bit
#define I2C_SLAVE_REGISTER_UVTH_VOLTAGE         0x20 // 32-bit
#define I2C_SLAVE_REGISTER_VIN_VOLTAGE          0xC0 // 32-bit
#define I2C_SLAVE_REGISTER_AVDD_VOLTAGE         0xD0 // 32-bit
#define I2C_SLAVE_REGISTER_DVDD_VOLTAGE         0xD4 // 32-bit
#define I2C_SLAVE_REGISTER_IOVDD_VOLTAGE        0xD8 // 32-bit
#define I2C_SLAVE_REGISTER_CORE_VOLTAGE         0xDC // 32-bit
#define I2C_SLAVE_REGISTER_EMU_TEMP             0xE0 // 32-bit
#define I2C_SLAVE_REGISTER_ADC_TEMP             0xE4 // 32-bit
#define I2C_SLAVE_REGISTER_SW_VERSION           0xF4 // 16-bit
#define I2C_SLAVE_REGISTER_DEV_UIDL             0xF8 // 32-bit
#define I2C_SLAVE_REGISTER_DEV_UIDH             0xFC // 32-bit

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

static void timers_init();

// Variables
volatile uint8_t ubI2CRegister[I2C_SLAVE_REGISTER_COUNT];
volatile uint8_t ubI2CRegisterWriteMask[I2C_SLAVE_REGISTER_COUNT];
volatile uint8_t ubI2CRegisterReadMask[I2C_SLAVE_REGISTER_COUNT];
volatile uint8_t ubI2CRegisterPointer = 0x00;
volatile uint8_t ubI2CFirstWrite = 1;
volatile uint32_t * const pulDutyCycleRegister[12] = {
    &(TIMER1->CC[1].CCV),
    &(TIMER1->CC[0].CCV),
    &(WTIMER0->CC[0].CCV),
    &(TIMER0->CC[2].CCV),
    &(TIMER0->CC[1].CCV),
    &(TIMER0->CC[0].CCV),
    &(TIMER1->CC[3].CCV),
    &(TIMER1->CC[2].CCV),
    &(WTIMER1->CC[1].CCV),
    &(WTIMER1->CC[0].CCV),
    &(WTIMER1->CC[3].CCV),
    &(WTIMER1->CC[2].CCV)
};

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
    ubI2CFirstWrite = 1;

    // Hardware address comparator already verifies if the address matches
    // This is only called if address is valid
    // All we need to do is ACK

    return 1; // ACK
}
uint8_t i2c_slave_tx_data_isr()
{
    uint8_t ubData = ubI2CRegister[ubI2CRegisterPointer] & ubI2CRegisterReadMask[ubI2CRegisterPointer];
    ubI2CRegisterPointer++;

    return ubData;
}
uint8_t i2c_slave_rx_data_isr(uint8_t ubData)
{
    if(ubI2CFirstWrite)
    {
        ubI2CRegisterPointer = ubData;
        ubI2CFirstWrite = 0;

        return 1; // ACK
    }

    ubI2CRegister[ubI2CRegisterPointer] = (ubI2CRegister[ubI2CRegisterPointer] & ~ubI2CRegisterWriteMask[ubI2CRegisterPointer]) | (ubData & ubI2CRegisterWriteMask[ubI2CRegisterPointer]);
    ubI2CRegisterPointer++;

    return 1; // ACK
}

void timers_init()
{
    // Timer 0
    cmu_hfper0_clock_gate(CMU_HFPERCLKEN0_TIMER0, 1);

    TIMER0->CTRL = TIMER_CTRL_RSSCOIST | TIMER_CTRL_PRESC_DIV4 | TIMER_CTRL_CLKSEL_PRESCHFPERCLK | TIMER_CTRL_FALLA_NONE | TIMER_CTRL_RISEA_NONE | TIMER_CTRL_MODE_UP;
    TIMER0->TOP = 0x00FF;
    TIMER0->CNT = 0x0000;

    //// R5
    TIMER0->CC[0].CTRL = TIMER_CC_CTRL_PRSCONF_LEVEL | TIMER_CC_CTRL_CUFOA_NONE | TIMER_CC_CTRL_COFOA_SET | TIMER_CC_CTRL_CMOA_CLEAR | TIMER_CC_CTRL_MODE_PWM;
    TIMER0->CC[0].CCV = 0x0000;
    //// R4
    TIMER0->CC[1].CTRL = TIMER_CC_CTRL_PRSCONF_LEVEL | TIMER_CC_CTRL_CUFOA_NONE | TIMER_CC_CTRL_COFOA_SET | TIMER_CC_CTRL_CMOA_CLEAR | TIMER_CC_CTRL_MODE_PWM;
    TIMER0->CC[1].CCV = 0x0000;
    //// R3
    TIMER0->CC[2].CTRL = TIMER_CC_CTRL_PRSCONF_LEVEL | TIMER_CC_CTRL_CUFOA_NONE | TIMER_CC_CTRL_COFOA_SET | TIMER_CC_CTRL_CMOA_CLEAR | TIMER_CC_CTRL_MODE_PWM;
    TIMER0->CC[2].CCV = 0x0000;

    TIMER0->ROUTELOC0 = TIMER_ROUTELOC0_CC2LOC_LOC0 | TIMER_ROUTELOC0_CC1LOC_LOC0 | TIMER_ROUTELOC0_CC0LOC_LOC0;
    TIMER0->ROUTEPEN |= TIMER_ROUTEPEN_CC2PEN | TIMER_ROUTEPEN_CC1PEN | TIMER_ROUTEPEN_CC0PEN;

    TIMER0->CMD = TIMER_CMD_START;

    // Timer 1
    cmu_hfper0_clock_gate(CMU_HFPERCLKEN0_TIMER1, 1);

    TIMER1->CTRL = TIMER_CTRL_RSSCOIST | TIMER_CTRL_PRESC_DIV2 | TIMER_CTRL_CLKSEL_PRESCHFPERCLK | TIMER_CTRL_FALLA_NONE | TIMER_CTRL_RISEA_NONE | TIMER_CTRL_MODE_UP;
    TIMER1->TOP = 0x00FF;
    TIMER1->CNT = 0x0000;

    //// R1
    TIMER1->CC[0].CTRL = TIMER_CC_CTRL_PRSCONF_LEVEL | TIMER_CC_CTRL_CUFOA_NONE | TIMER_CC_CTRL_COFOA_SET | TIMER_CC_CTRL_CMOA_CLEAR | TIMER_CC_CTRL_MODE_PWM;
    TIMER1->CC[0].CCV = 0x0000;
    //// R0
    TIMER1->CC[1].CTRL = TIMER_CC_CTRL_PRSCONF_LEVEL | TIMER_CC_CTRL_CUFOA_NONE | TIMER_CC_CTRL_COFOA_SET | TIMER_CC_CTRL_CMOA_CLEAR | TIMER_CC_CTRL_MODE_PWM;
    TIMER1->CC[1].CCV = 0x0000;
    //// R7
    TIMER1->CC[2].CTRL = TIMER_CC_CTRL_PRSCONF_LEVEL | TIMER_CC_CTRL_CUFOA_NONE | TIMER_CC_CTRL_COFOA_SET | TIMER_CC_CTRL_CMOA_CLEAR | TIMER_CC_CTRL_MODE_PWM;
    TIMER1->CC[2].CCV = 0x0000;
    //// R6
    TIMER1->CC[3].CTRL = TIMER_CC_CTRL_PRSCONF_LEVEL | TIMER_CC_CTRL_CUFOA_NONE | TIMER_CC_CTRL_COFOA_SET | TIMER_CC_CTRL_CMOA_CLEAR | TIMER_CC_CTRL_MODE_PWM;
    TIMER1->CC[3].CCV = 0x0000;

    TIMER1->ROUTELOC0 = TIMER_ROUTELOC0_CC3LOC_LOC4 | TIMER_ROUTELOC0_CC2LOC_LOC4 | TIMER_ROUTELOC0_CC1LOC_LOC3 | TIMER_ROUTELOC0_CC0LOC_LOC3;
    TIMER1->ROUTEPEN |= TIMER_ROUTEPEN_CC3PEN | TIMER_ROUTEPEN_CC2PEN | TIMER_ROUTEPEN_CC1PEN | TIMER_ROUTEPEN_CC0PEN;

    TIMER1->CMD = TIMER_CMD_START;

    // Wide Timer 0
    cmu_hfper1_clock_gate(CMU_HFPERCLKEN1_WTIMER0, 1);

    WTIMER0->CTRL = WTIMER_CTRL_RSSCOIST | WTIMER_CTRL_PRESC_DIV2 | WTIMER_CTRL_CLKSEL_PRESCHFPERCLK | WTIMER_CTRL_FALLA_NONE | WTIMER_CTRL_RISEA_NONE | WTIMER_CTRL_MODE_UP;
    WTIMER0->TOP = 0x00FF;
    WTIMER0->CNT = 0x0000;

    //// R2
    WTIMER0->CC[0].CTRL = WTIMER_CC_CTRL_PRSCONF_LEVEL | WTIMER_CC_CTRL_CUFOA_NONE | WTIMER_CC_CTRL_COFOA_SET | WTIMER_CC_CTRL_CMOA_CLEAR | WTIMER_CC_CTRL_MODE_PWM;
    WTIMER0->CC[0].CCV = 0x0000;

    WTIMER0->ROUTELOC0 = WTIMER_ROUTELOC0_CC0LOC_LOC7;
    WTIMER0->ROUTEPEN |= WTIMER_ROUTEPEN_CC0PEN;

    WTIMER0->CMD = WTIMER_CMD_START;

    // Wide Timer 1
    cmu_hfper1_clock_gate(CMU_HFPERCLKEN1_WTIMER1, 1);

    WTIMER1->CTRL = WTIMER_CTRL_RSSCOIST | WTIMER_CTRL_PRESC_DIV2 | WTIMER_CTRL_CLKSEL_PRESCHFPERCLK | WTIMER_CTRL_FALLA_NONE | WTIMER_CTRL_RISEA_NONE | WTIMER_CTRL_MODE_UP;
    WTIMER1->TOP = 0x00FF;
    WTIMER1->CNT = 0x0000;

    //// R9
    WTIMER1->CC[0].CTRL = WTIMER_CC_CTRL_PRSCONF_LEVEL | WTIMER_CC_CTRL_CUFOA_NONE | WTIMER_CC_CTRL_COFOA_SET | WTIMER_CC_CTRL_CMOA_CLEAR | WTIMER_CC_CTRL_MODE_PWM;
    WTIMER1->CC[0].CCV = 0x0000;
    //// R8
    WTIMER1->CC[1].CTRL = WTIMER_CC_CTRL_PRSCONF_LEVEL | WTIMER_CC_CTRL_CUFOA_NONE | WTIMER_CC_CTRL_COFOA_SET | WTIMER_CC_CTRL_CMOA_CLEAR | WTIMER_CC_CTRL_MODE_PWM;
    WTIMER1->CC[1].CCV = 0x0000;
    //// R11
    WTIMER1->CC[2].CTRL = WTIMER_CC_CTRL_PRSCONF_LEVEL | WTIMER_CC_CTRL_CUFOA_NONE | WTIMER_CC_CTRL_COFOA_SET | WTIMER_CC_CTRL_CMOA_CLEAR | WTIMER_CC_CTRL_MODE_PWM;
    WTIMER1->CC[2].CCV = 0x0000;
    //// R10
    WTIMER1->CC[3].CTRL = WTIMER_CC_CTRL_PRSCONF_LEVEL | WTIMER_CC_CTRL_CUFOA_NONE | WTIMER_CC_CTRL_COFOA_SET | WTIMER_CC_CTRL_CMOA_CLEAR | WTIMER_CC_CTRL_MODE_PWM;
    WTIMER1->CC[3].CCV = 0x0000;

    WTIMER1->ROUTELOC0 = WTIMER_ROUTELOC0_CC3LOC_LOC1 | WTIMER_ROUTELOC0_CC2LOC_LOC1 | WTIMER_ROUTELOC0_CC1LOC_LOC2 | WTIMER_ROUTELOC0_CC0LOC_LOC2;
    WTIMER1->ROUTEPEN |= WTIMER_ROUTEPEN_CC3PEN | WTIMER_ROUTEPEN_CC2PEN | WTIMER_ROUTEPEN_CC1PEN | WTIMER_ROUTEPEN_CC0PEN;

    WTIMER1->CMD = WTIMER_CMD_START;
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

    i2c0_init(I2C_SLAVE_ADDRESS, 6, 6);
    i2c0_set_slave_addr_isr(i2c_slave_addr_isr);
    i2c0_set_slave_tx_data_isr(i2c_slave_tx_data_isr);
    i2c0_set_slave_rx_data_isr(i2c_slave_rx_data_isr);

    char szDeviceName[32];

    get_device_name(szDeviceName, 32);

    DBGPRINTLN_CTX("IcyRadio Relay Controller v%lu (%s %s)!", BUILD_VERSION, __DATE__, __TIME__);
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

    return 0;
}
int main()
{
    // Timers
    timers_init();

    // I2C Slave Register block
    i2c_slave_register_init();

    while(1)
    {
        wdog_feed();

        static uint64_t ullLastLEDTick = 0;
        static uint64_t ullLastDebugPrint = 0;

        if(g_ullSystemTick - ullLastLEDTick > 2000)
        {
            ullLastLEDTick = g_ullSystemTick;

            LED_HIGH();
            delay_ms(50);
            LED_LOW();
        }

        if(g_ullSystemTick - ullLastDebugPrint > 5000)
        {
            ullLastDebugPrint = g_ullSystemTick;

            float fADCTemp = adc_get_temperature();
            float fEMUTemp = emu_get_temperature();

            float fVIN = adc_get_vin();
            float fAVDD = adc_get_avdd();
            float fDVDD = adc_get_dvdd();
            float fIOVDD = adc_get_iovdd();
            float fCoreVDD = adc_get_corevdd();

            ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
            {
                I2C_SLAVE_REGISTER(float, I2C_SLAVE_REGISTER_ADC_TEMP) = fADCTemp;
                I2C_SLAVE_REGISTER(float, I2C_SLAVE_REGISTER_EMU_TEMP) = fEMUTemp;

                I2C_SLAVE_REGISTER(float, I2C_SLAVE_REGISTER_VIN_VOLTAGE) = fVIN;
                I2C_SLAVE_REGISTER(float, I2C_SLAVE_REGISTER_AVDD_VOLTAGE) = fAVDD;
                I2C_SLAVE_REGISTER(float, I2C_SLAVE_REGISTER_DVDD_VOLTAGE) = fDVDD;
                I2C_SLAVE_REGISTER(float, I2C_SLAVE_REGISTER_IOVDD_VOLTAGE) = fIOVDD;
                I2C_SLAVE_REGISTER(float, I2C_SLAVE_REGISTER_CORE_VOLTAGE) = fCoreVDD;
            }

            DBGPRINTLN_CTX("----------------------------------");
            DBGPRINTLN_CTX("ADC temperature: %.2f C", fADCTemp);
            DBGPRINTLN_CTX("EMU temperature: %.2f C", fEMUTemp);
            DBGPRINTLN_CTX("AVDD Voltage: %.2f mV", fAVDD);
            DBGPRINTLN_CTX("DVDD Voltage: %.2f mV", fDVDD);
            DBGPRINTLN_CTX("IOVDD Voltage: %.2f mV", fIOVDD);
            DBGPRINTLN_CTX("Core Voltage: %.2f mV", fCoreVDD);
            DBGPRINTLN_CTX("VIN Voltage: %.2f mV", fVIN);

            for(int i = 0; i < 12; i++)
            {
                wdog_feed();

                if(*pulDutyCycleRegister[i])
                {
                    for(int j = *pulDutyCycleRegister[i]; j > 0; j -= 10)
                    {
                        *pulDutyCycleRegister[i] = j > 0 ? j : 0;

                        delay_ms(20);
                    }

                    *pulDutyCycleRegister[i] = 0;

                    continue;
                }

                for(int j = 0; j < 256; j += 10)
                {
                    *pulDutyCycleRegister[i] = j <= 255 ? j : 255;

                    delay_ms(20);
                }

                *pulDutyCycleRegister[i] = 255;
            }
        }
    }

    return 0;
}