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
#include "adf4351.h"
#include "f1958.h"
#include "i2c.h"
#include "mcp3421.h"
#include "wdog.h"

// Structs

// Helper macros
#define I2C_SLAVE_ADDRESS                           0x3D
#define I2C_SLAVE_REGISTER_COUNT                    256
#define I2C_SLAVE_REGISTER(t, a)                    (*(t *)&ubI2CRegister[(a)])
#define I2C_SLAVE_REGISTER_WRITE_MASK(t, a)         (*(t *)&ubI2CRegisterWriteMask[(a)])
#define I2C_SLAVE_REGISTER_READ_MASK(t, a)          (*(t *)&ubI2CRegisterReadMask[(a)])
#define I2C_SLAVE_REGISTER_STATUS                   0x00 // 8-bit
#define I2C_SLAVE_REGISTER_CONFIG                   0x01 // 8-bit
#define I2C_SLAVE_REGISTER_LO_FREQ                  0x10 // 64-bit
#define I2C_SLAVE_REGISTER_LO_REF_FREQ              0x18 // 32-bit
#define I2C_SLAVE_REGISTER_LO_PFD_FREQ              0x1C // 32-bit
#define I2C_SLAVE_REGISTER_IF_ATT                   0x20 // 32-bit
#define I2C_SLAVE_REGISTER_RF1_ATT                  0x24 // 32-bit
#define I2C_SLAVE_REGISTER_RF2_ATT                  0x28 // 32-bit
#define I2C_SLAVE_REGISTER_RF_OUT_PWR_STATUS        0x30 // 8-bit
#define I2C_SLAVE_REGISTER_RF_OUT_PWR_CONFIG        0x31 // 8-bit
#define I2C_SLAVE_REGISTER_RF_OUT_PWR_LOW_THRESH    0x34 // 32-bit
#define I2C_SLAVE_REGISTER_RF_OUT_PWR               0x38 // 32-bit
#define I2C_SLAVE_REGISTER_VIN_VOLTAGE              0xC0 // 32-bit
#define I2C_SLAVE_REGISTER_5V0_VOLTAGE              0xC4 // 32-bit
#define I2C_SLAVE_REGISTER_5V0_CURRENT              0xC8 // 32-bit
#define I2C_SLAVE_REGISTER_AVDD_VOLTAGE             0xD0 // 32-bit
#define I2C_SLAVE_REGISTER_DVDD_VOLTAGE             0xD4 // 32-bit
#define I2C_SLAVE_REGISTER_IOVDD_VOLTAGE            0xD8 // 32-bit
#define I2C_SLAVE_REGISTER_CORE_VOLTAGE             0xDC // 32-bit
#define I2C_SLAVE_REGISTER_EMU_TEMP                 0xE0 // 32-bit
#define I2C_SLAVE_REGISTER_ADC_TEMP                 0xE4 // 32-bit
#define I2C_SLAVE_REGISTER_SW_VERSION               0xF4 // 16-bit
#define I2C_SLAVE_REGISTER_DEV_UIDL                 0xF8 // 32-bit
#define I2C_SLAVE_REGISTER_DEV_UIDH                 0xFC // 32-bit

//// Power meter calibration tables
// CW
#define MOD_CW                      fPData_CW, fVData_CW, (sizeof(fPData_CW) / sizeof(float))
static const float fPData_CW[]      = {-10.50, -9.70, -8.60, -7.57, -6.50, -5.50, -0.80, 3.80, 8.75, 13.70, 18.25, 21.90, 22.70, 22.90, 23.37, 23.80};
static const float fVData_CW[]      = {1.52, 22.68, 59.00, 93.10, 126.50, 160.23, 303.73, 439.65, 581.63, 719.60, 853.53, 990.86, 1014.28, 1043.68, 1067.15, 1071.76};
// QPSK
#define MOD_QPSK                    fPData_QPSK, fVData_QPSK, (sizeof(fPData_QPSK) / sizeof(float))
static const float fPData_QPSK[]    = {-10.50, -9.70, -8.60, -7.57, -6.50, -5.50, -0.80, 3.80, 8.75, 13.70, 18.25, 21.90, 22.70, 22.90, 23.37, 23.80};
static const float fVData_QPSK[]    = {1.52, 22.68, 59.00, 93.10, 126.50, 160.23, 303.73, 439.65, 581.63, 719.60, 853.53, 990.86, 1014.28, 1043.68, 1067.15, 1071.76};
// 8PSK,
#define MOD_8PSK                    fPData_8PSK, fVData_8PSK, (sizeof(fPData_8PSK) / sizeof(float))
static const float fPData_8PSK[]    = {-10.65, -9.73, -8.65, -7.38, -6.31, -5.93, -1.20, 3.42, 8.41, 13.30, 17.77, 21.53, 22.16, 22.80, 23.15, 23.67};
static const float fVData_8PSK[]    = {1.47, 20.89, 55.34, 89.95, 122.35, 156.76, 299.89, 435.51, 578.40, 716.68, 850.36, 986.79, 1011.46, 1041.43, 1064.18, 1071.47};
// 16APSK
#define MOD_16APSK                  fPData_16APSK, fVData_16APSK, (sizeof(fPData_16APSK) / sizeof(float))
static const float fPData_16APSK[]  = {-10.68, -9.97, -8.83, -7.77, -6.77, -5.73, -1.06, 3.45, 8.45, 13.51, 17.97, 21.61, 22.15, 22.67, 22.93, 23.90};
static const float fVData_16APSK[]  = {1.43, 25.62, 61.63, 95.87, 128.30, 159.84, 302.70, 436.33, 579.06, 717.97, 850.71, 988.16, 1011.32, 1036.59, 1059.04, 1070.22};
// 32APSK
#define MOD_32APSK                  fPData_32APSK, fVData_32APSK, (sizeof(fPData_32APSK) / sizeof(float))
static const float fPData_32APSK[]  = {-10.56, -10.00, -8.80, -7.90, -6.66, -5.53, -0.88, 3.81, 8.75, 13.70, 18.13, 21.85, 22.30, 22.86, 23.35, 23.90};
static const float fVData_32APSK[]  = {1.41, 16.76, 52.18, 84.60, 126.12, 159.64, 302.23, 438.64, 581.43, 720.25, 853.64, 898.74, 1013.76, 1041.22, 1063.64, 1071.27};

// Forward declarations
static void reset() __attribute__((noreturn));
static void sleep();

static uint32_t get_free_ram();

static void get_device_core_name(char *pszDeviceCoreName, uint32_t ulDeviceCoreNameSize);
static void get_device_name(char *pszDeviceName, uint32_t ulDeviceNameSize);
static uint16_t get_device_revision();

static void wdog_warning_isr();

static void i2c_slave_register_init();
static uint8_t i2c_slave_addr_isr(uint8_t ubRnW);
static uint8_t i2c_slave_tx_data_isr();
static uint8_t i2c_slave_rx_data_isr(uint8_t ubData);

static void acmp_init();
static void acmp_set_rf_pwr_thresh(float fUTP, float fLTP);
static void acmp_get_rf_pwr_thresh(float *pfUTP, float *pfLTP);

static float ext_adc_get_5v0_current(uint32_t ulSamples);

static float interp_rf_out_power(float fVoltage, const float *pfPData, const float *pfVData, uint32_t ulDataSize);
static float interp_rf_out_power_inv(float fPower, const float *pfPData, const float *pfVData, uint32_t ulDataSize);
static float get_rf_out_power(uint32_t ulSamples, const float *pfPData, const float *pfVData, uint32_t ulDataSize);

// Variables
volatile uint8_t ubI2CRegister[I2C_SLAVE_REGISTER_COUNT];
volatile uint8_t ubI2CRegisterWriteMask[I2C_SLAVE_REGISTER_COUNT];
volatile uint8_t ubI2CRegisterReadMask[I2C_SLAVE_REGISTER_COUNT];
volatile uint8_t ubI2CRegisterPointer = 0x00;
volatile uint8_t ubI2CByteCount = 0;
volatile uint8_t ublLOChanged = 0;
volatile uint8_t ubAttChanged = 0;
volatile uint8_t ubRFPowerMeterChanged = 0;
const float * pfRFPowerMeterPData = fPData_CW;
const float * pfRFPowerMeterVData = fVData_CW;
uint32_t ulRFPowerMeterDataSize = sizeof(fPData_CW) / sizeof(float);

// ISRs
void _acmp0_1_isr()
{
    uint32_t ulFlags = ACMP0->IFC;

    if(ulFlags & ACMP_IFC_EDGE)
    {
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
        {
            if(ACMP0->STATUS & ACMP_STATUS_ACMPOUT)
            {
                I2C_SLAVE_REGISTER(uint8_t, I2C_SLAVE_REGISTER_RF_OUT_PWR_STATUS) |= BIT(1);
            }
            else
            {
                if(I2C_SLAVE_REGISTER(uint8_t, I2C_SLAVE_REGISTER_RF_OUT_PWR_CONFIG) & BIT(4)) // Turn off amplifiers ASAP on low RF power event
                {
                    I2C_SLAVE_REGISTER(uint8_t, I2C_SLAVE_REGISTER_CONFIG) &= ~(BIT(2) | BIT(3));

                    PA_STG3_DISABLE();
                    PA_STG1_2_DISABLE();
                }

                I2C_SLAVE_REGISTER(uint8_t, I2C_SLAVE_REGISTER_RF_OUT_PWR_STATUS) &= ~BIT(1);
                I2C_SLAVE_REGISTER(uint8_t, I2C_SLAVE_REGISTER_RF_OUT_PWR_STATUS) |= BIT(0);
            }
        }
    }
}

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

void get_device_core_name(char *pszDeviceCoreName, uint32_t ulDeviceCoreNameSize)
{
    uint8_t ubImplementer = (SCB->CPUID & SCB_CPUID_IMPLEMENTER_Msk) >> SCB_CPUID_IMPLEMENTER_Pos;
    const char* szImplementer = "?";

    switch(ubImplementer)
    {
        case 0x41: szImplementer = "ARM"; break;
    }

    uint16_t usPartNo = (SCB->CPUID & SCB_CPUID_PARTNO_Msk) >> SCB_CPUID_PARTNO_Pos;
    const char* szPartNo = "?";

    switch(usPartNo)
    {
        case 0xC20: szPartNo = "Cortex-M0"; break;
        case 0xC60: szPartNo = "Cortex-M0+"; break;
        case 0xC21: szPartNo = "Cortex-M1"; break;
        case 0xD20: szPartNo = "Cortex-M23"; break;
        case 0xC23: szPartNo = "Cortex-M3"; break;
        case 0xD21: szPartNo = "Cortex-M33"; break;
        case 0xC24: szPartNo = "Cortex-M4"; break;
        case 0xC27: szPartNo = "Cortex-M7"; break;
    }

    uint8_t ubVariant = (SCB->CPUID & SCB_CPUID_VARIANT_Msk) >> SCB_CPUID_VARIANT_Pos;
    uint8_t ubRevision = (SCB->CPUID & SCB_CPUID_REVISION_Msk) >> SCB_CPUID_REVISION_Pos;

    snprintf(pszDeviceCoreName, ulDeviceCoreNameSize, "%s %s r%hhup%hhu", szImplementer, szPartNo, ubVariant, ubRevision);
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
        I2C_SLAVE_REGISTER              (uint8_t,  I2C_SLAVE_REGISTER_STATUS)                   = 0x00;
        I2C_SLAVE_REGISTER_WRITE_MASK   (uint8_t,  I2C_SLAVE_REGISTER_STATUS)                   = 0x00;
        I2C_SLAVE_REGISTER_READ_MASK    (uint8_t,  I2C_SLAVE_REGISTER_STATUS)                   = 0xFF;
        I2C_SLAVE_REGISTER              (uint8_t,  I2C_SLAVE_REGISTER_CONFIG)                   = 0x01;
        I2C_SLAVE_REGISTER_WRITE_MASK   (uint8_t,  I2C_SLAVE_REGISTER_CONFIG)                   = 0xFF;
        I2C_SLAVE_REGISTER_READ_MASK    (uint8_t,  I2C_SLAVE_REGISTER_CONFIG)                   = 0xFF;
        I2C_SLAVE_REGISTER              (uint64_t, I2C_SLAVE_REGISTER_LO_FREQ)                  = 1875000000U;
        I2C_SLAVE_REGISTER_WRITE_MASK   (uint64_t, I2C_SLAVE_REGISTER_LO_FREQ)                  = 0xFFFFFFFFFFFFFFFF;
        I2C_SLAVE_REGISTER_READ_MASK    (uint64_t, I2C_SLAVE_REGISTER_LO_FREQ)                  = 0xFFFFFFFFFFFFFFFF;
        I2C_SLAVE_REGISTER              (uint32_t, I2C_SLAVE_REGISTER_LO_REF_FREQ)              = 26000000;
        I2C_SLAVE_REGISTER_WRITE_MASK   (uint32_t, I2C_SLAVE_REGISTER_LO_REF_FREQ)              = 0x00000000; // TODO: Allow PLL reconfiguration
        I2C_SLAVE_REGISTER_READ_MASK    (uint32_t, I2C_SLAVE_REGISTER_LO_REF_FREQ)              = 0xFFFFFFFF;
        I2C_SLAVE_REGISTER              (uint32_t, I2C_SLAVE_REGISTER_LO_PFD_FREQ)              = 1000000;
        I2C_SLAVE_REGISTER_WRITE_MASK   (uint32_t, I2C_SLAVE_REGISTER_LO_PFD_FREQ)              = 0x00000000; // TODO: Allow PLL reconfiguration
        I2C_SLAVE_REGISTER_READ_MASK    (uint32_t, I2C_SLAVE_REGISTER_LO_PFD_FREQ)              = 0xFFFFFFFF;
        I2C_SLAVE_REGISTER              (float,    I2C_SLAVE_REGISTER_IF_ATT)                   = 32.75f;
        I2C_SLAVE_REGISTER_WRITE_MASK   (uint32_t, I2C_SLAVE_REGISTER_IF_ATT)                   = 0xFFFFFFFF;
        I2C_SLAVE_REGISTER_READ_MASK    (uint32_t, I2C_SLAVE_REGISTER_IF_ATT)                   = 0xFFFFFFFF;
        I2C_SLAVE_REGISTER              (float,    I2C_SLAVE_REGISTER_RF1_ATT)                  = 32.75f;
        I2C_SLAVE_REGISTER_WRITE_MASK   (uint32_t, I2C_SLAVE_REGISTER_RF1_ATT)                  = 0xFFFFFFFF;
        I2C_SLAVE_REGISTER_READ_MASK    (uint32_t, I2C_SLAVE_REGISTER_RF1_ATT)                  = 0xFFFFFFFF;
        I2C_SLAVE_REGISTER              (float,    I2C_SLAVE_REGISTER_RF2_ATT)                  = 32.75f;
        I2C_SLAVE_REGISTER_WRITE_MASK   (uint32_t, I2C_SLAVE_REGISTER_RF2_ATT)                  = 0xFFFFFFFF;
        I2C_SLAVE_REGISTER_READ_MASK    (uint32_t, I2C_SLAVE_REGISTER_RF2_ATT)                  = 0xFFFFFFFF;
        I2C_SLAVE_REGISTER              (uint8_t,  I2C_SLAVE_REGISTER_RF_OUT_PWR_CONFIG)        = 0x00;
        I2C_SLAVE_REGISTER_WRITE_MASK   (uint8_t,  I2C_SLAVE_REGISTER_RF_OUT_PWR_CONFIG)        = 0xFF;
        I2C_SLAVE_REGISTER_READ_MASK    (uint8_t,  I2C_SLAVE_REGISTER_RF_OUT_PWR_CONFIG)        = 0xFF;
        I2C_SLAVE_REGISTER              (float,    I2C_SLAVE_REGISTER_RF_OUT_PWR_LOW_THRESH)    = -99.f;
        I2C_SLAVE_REGISTER_WRITE_MASK   (uint32_t, I2C_SLAVE_REGISTER_RF_OUT_PWR_LOW_THRESH)    = 0xFFFFFFFF;
        I2C_SLAVE_REGISTER_READ_MASK    (uint32_t, I2C_SLAVE_REGISTER_RF_OUT_PWR_LOW_THRESH)    = 0xFFFFFFFF;
        I2C_SLAVE_REGISTER              (float,    I2C_SLAVE_REGISTER_RF_OUT_PWR)               = -99.f;
        I2C_SLAVE_REGISTER_WRITE_MASK   (uint32_t, I2C_SLAVE_REGISTER_RF_OUT_PWR)               = 0x00000000;
        I2C_SLAVE_REGISTER_READ_MASK    (uint32_t, I2C_SLAVE_REGISTER_RF_OUT_PWR)               = 0xFFFFFFFF;
        I2C_SLAVE_REGISTER              (float,    I2C_SLAVE_REGISTER_VIN_VOLTAGE)              = -1.f;
        I2C_SLAVE_REGISTER_WRITE_MASK   (uint32_t, I2C_SLAVE_REGISTER_VIN_VOLTAGE)              = 0x00000000;
        I2C_SLAVE_REGISTER_READ_MASK    (uint32_t, I2C_SLAVE_REGISTER_VIN_VOLTAGE)              = 0xFFFFFFFF;
        I2C_SLAVE_REGISTER              (float,    I2C_SLAVE_REGISTER_5V0_VOLTAGE)              = -1.f;
        I2C_SLAVE_REGISTER_WRITE_MASK   (uint32_t, I2C_SLAVE_REGISTER_5V0_VOLTAGE)              = 0x00000000;
        I2C_SLAVE_REGISTER_READ_MASK    (uint32_t, I2C_SLAVE_REGISTER_5V0_VOLTAGE)              = 0xFFFFFFFF;
        I2C_SLAVE_REGISTER              (float,    I2C_SLAVE_REGISTER_5V0_CURRENT)              = -1.f;
        I2C_SLAVE_REGISTER_WRITE_MASK   (uint32_t, I2C_SLAVE_REGISTER_5V0_CURRENT)              = 0x00000000;
        I2C_SLAVE_REGISTER_READ_MASK    (uint32_t, I2C_SLAVE_REGISTER_5V0_CURRENT)              = 0xFFFFFFFF;
        I2C_SLAVE_REGISTER              (float,    I2C_SLAVE_REGISTER_AVDD_VOLTAGE)             = -1.f;
        I2C_SLAVE_REGISTER_WRITE_MASK   (uint32_t, I2C_SLAVE_REGISTER_AVDD_VOLTAGE)             = 0x00000000;
        I2C_SLAVE_REGISTER_READ_MASK    (uint32_t, I2C_SLAVE_REGISTER_AVDD_VOLTAGE)             = 0xFFFFFFFF;
        I2C_SLAVE_REGISTER              (float,    I2C_SLAVE_REGISTER_DVDD_VOLTAGE)             = -1.f;
        I2C_SLAVE_REGISTER_WRITE_MASK   (uint32_t, I2C_SLAVE_REGISTER_DVDD_VOLTAGE)             = 0x00000000;
        I2C_SLAVE_REGISTER_READ_MASK    (uint32_t, I2C_SLAVE_REGISTER_DVDD_VOLTAGE)             = 0xFFFFFFFF;
        I2C_SLAVE_REGISTER              (float,    I2C_SLAVE_REGISTER_IOVDD_VOLTAGE)            = -1.f;
        I2C_SLAVE_REGISTER_WRITE_MASK   (uint32_t, I2C_SLAVE_REGISTER_IOVDD_VOLTAGE)            = 0x00000000;
        I2C_SLAVE_REGISTER_READ_MASK    (uint32_t, I2C_SLAVE_REGISTER_IOVDD_VOLTAGE)            = 0xFFFFFFFF;
        I2C_SLAVE_REGISTER              (float,    I2C_SLAVE_REGISTER_CORE_VOLTAGE)             = -1.f;
        I2C_SLAVE_REGISTER_WRITE_MASK   (uint32_t, I2C_SLAVE_REGISTER_CORE_VOLTAGE)             = 0x00000000;
        I2C_SLAVE_REGISTER_READ_MASK    (uint32_t, I2C_SLAVE_REGISTER_CORE_VOLTAGE)             = 0xFFFFFFFF;
        I2C_SLAVE_REGISTER              (float,    I2C_SLAVE_REGISTER_EMU_TEMP)                 = -1.f;
        I2C_SLAVE_REGISTER_WRITE_MASK   (uint32_t, I2C_SLAVE_REGISTER_EMU_TEMP)                 = 0x00000000;
        I2C_SLAVE_REGISTER_READ_MASK    (uint32_t, I2C_SLAVE_REGISTER_EMU_TEMP)                 = 0xFFFFFFFF;
        I2C_SLAVE_REGISTER              (float,    I2C_SLAVE_REGISTER_ADC_TEMP)                 = -1.f;
        I2C_SLAVE_REGISTER_WRITE_MASK   (uint32_t, I2C_SLAVE_REGISTER_ADC_TEMP)                 = 0x00000000;
        I2C_SLAVE_REGISTER_READ_MASK    (uint32_t, I2C_SLAVE_REGISTER_ADC_TEMP)                 = 0xFFFFFFFF;
        I2C_SLAVE_REGISTER              (uint16_t, I2C_SLAVE_REGISTER_SW_VERSION)               = BUILD_VERSION;
        I2C_SLAVE_REGISTER_WRITE_MASK   (uint16_t, I2C_SLAVE_REGISTER_SW_VERSION)               = 0x0000;
        I2C_SLAVE_REGISTER_READ_MASK    (uint16_t, I2C_SLAVE_REGISTER_SW_VERSION)               = 0xFFFF;
        I2C_SLAVE_REGISTER              (uint32_t, I2C_SLAVE_REGISTER_DEV_UIDL)                 = DEVINFO->UNIQUEL;
        I2C_SLAVE_REGISTER_WRITE_MASK   (uint32_t, I2C_SLAVE_REGISTER_DEV_UIDL)                 = 0x00000000;
        I2C_SLAVE_REGISTER_READ_MASK    (uint32_t, I2C_SLAVE_REGISTER_DEV_UIDL)                 = 0xFFFFFFFF;
        I2C_SLAVE_REGISTER              (uint32_t, I2C_SLAVE_REGISTER_DEV_UIDH)                 = DEVINFO->UNIQUEH;
        I2C_SLAVE_REGISTER_WRITE_MASK   (uint32_t, I2C_SLAVE_REGISTER_DEV_UIDH)                 = 0x00000000;
        I2C_SLAVE_REGISTER_READ_MASK    (uint32_t, I2C_SLAVE_REGISTER_DEV_UIDH)                 = 0xFFFFFFFF;
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

    switch(ubI2CRegisterPointer)
    {
        case I2C_SLAVE_REGISTER_RF_OUT_PWR_STATUS + sizeof(uint8_t):
        {
            if(ubI2CByteCount < sizeof(uint8_t))
                break;

            I2C_SLAVE_REGISTER(uint8_t, I2C_SLAVE_REGISTER_RF_OUT_PWR_STATUS) &= ~BIT(0); // Clear on read
        }
        break;
    }

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

    switch(ubI2CRegisterPointer)
    {
        case I2C_SLAVE_REGISTER_LO_FREQ + sizeof(uint64_t):
        {
            if((ubI2CByteCount - 1) < sizeof(uint64_t))
                break;

            ublLOChanged = 1;
        }
        break;
        case I2C_SLAVE_REGISTER_IF_ATT + sizeof(uint32_t):
        {
            if((ubI2CByteCount - 1) < sizeof(uint32_t))
                break;

            ubAttChanged |= BIT(F1958_IF_ATT_ID);
        }
        break;
        case I2C_SLAVE_REGISTER_RF1_ATT + sizeof(uint32_t):
        {
            if((ubI2CByteCount - 1) < sizeof(uint32_t))
                break;

            ubAttChanged |= BIT(F1958_RF1_ATT_ID);
        }
        break;
        case I2C_SLAVE_REGISTER_RF2_ATT + sizeof(uint32_t):
        {
            if((ubI2CByteCount - 1) < sizeof(uint32_t))
                break;

            ubAttChanged |= BIT(F1958_RF2_ATT_ID);
        }
        break;
        case I2C_SLAVE_REGISTER_RF_OUT_PWR_CONFIG + sizeof(uint8_t):
        {
            if((ubI2CByteCount - 1) < sizeof(uint8_t))
                break;

            ubRFPowerMeterChanged = 1;
        }
        break;
        case I2C_SLAVE_REGISTER_RF_OUT_PWR_LOW_THRESH + sizeof(uint32_t):
        {
            if((ubI2CByteCount - 1) < sizeof(uint32_t))
                break;

            ubRFPowerMeterChanged = 1;
        }
        break;
    }

    return 1; // ACK
}

void acmp_init()
{
    cmu_hfper0_clock_gate(CMU_HFPERCLKEN0_ACMP0, 1);

    ACMP0->CTRL = ACMP_CTRL_FULLBIAS | (0x20 << _ACMP_CTRL_BIASPROG_SHIFT) | ACMP_CTRL_IFALL | ACMP_CTRL_IRISE | ACMP_CTRL_INPUTRANGE_FULL | ACMP_CTRL_ACCURACY_HIGH | ACMP_CTRL_PWRSEL_AVDD | ACMP_CTRL_GPIOINV_NOTINV | ACMP_CTRL_INACTVAL_LOW;
    ACMP0->INPUTSEL = ACMP_INPUTSEL_VBSEL_2V5 | ACMP_INPUTSEL_VASEL_APORT2YCH0 | ACMP_INPUTSEL_NEGSEL_VBDIV | ACMP_INPUTSEL_POSSEL_VADIV;

    ACMP0->IFC = _ACMP_IFC_MASK; // Clear pending IRQs
    IRQ_CLEAR(ACMP0_IRQn); // Clear pending vector
    IRQ_SET_PRIO(ACMP0_IRQn, 3, 0); // Set priority 3,0
    IRQ_ENABLE(ACMP0_IRQn); // Enable vector
    ACMP0->IEN |= ACMP_IEN_EDGE; // Enable EDGE interrupt

    ACMP0->CTRL |= ACMP_CTRL_EN; // Enable ACMP0
    while(!(ACMP0->STATUS & ACMP_STATUS_ACMPACT)); // Wait for it to be enabled
}
void acmp_set_rf_pwr_thresh(float fUTP, float fLTP)
{
    // Convert power to voltage (at the MCU pin)
    fUTP = interp_rf_out_power_inv(fUTP, pfRFPowerMeterPData, pfRFPowerMeterVData, ulRFPowerMeterDataSize);
    fLTP = interp_rf_out_power_inv(fLTP, pfRFPowerMeterPData, pfRFPowerMeterVData, ulRFPowerMeterDataSize);

    // Apply VADIV factor
    fUTP *= 32.f / 64.f;
    fLTP *= 32.f / 64.f;

    if(fUTP < 2500.f * 1.f / 64.f || fUTP > 2500.f)
        return;

    if(fLTP < 2500.f * 1.f / 64.f || fLTP > 2500.f)
        return;

    if(fLTP > fUTP)
        return;

    ACMP0->HYSTERESIS0 = (((uint8_t)(fUTP * 64.f / 2500.f) - 1) << _ACMP_HYSTERESIS0_DIVVB_SHIFT) | ((32 - 1) << _ACMP_HYSTERESIS0_DIVVA_SHIFT);
    ACMP0->HYSTERESIS1 = (((uint8_t)(fLTP * 64.f / 2500.f) - 1) << _ACMP_HYSTERESIS1_DIVVB_SHIFT) | ((32 - 1) << _ACMP_HYSTERESIS1_DIVVA_SHIFT);
}
void acmp_get_rf_pwr_thresh(float *pfUTP, float *pfLTP)
{
    if(pfUTP)
    {
        uint8_t ubVACode = (ACMP0->HYSTERESIS0 & _ACMP_HYSTERESIS0_DIVVA_MASK) >> _ACMP_HYSTERESIS0_DIVVA_SHIFT;
        uint8_t ubVBCode = (ACMP0->HYSTERESIS0 & _ACMP_HYSTERESIS0_DIVVB_MASK) >> _ACMP_HYSTERESIS0_DIVVB_SHIFT;

        *pfUTP = interp_rf_out_power(2500.f * (float)(ubVBCode + 1) / (float)(ubVACode + 1), pfRFPowerMeterPData, pfRFPowerMeterVData, ulRFPowerMeterDataSize);
    }

    if(pfLTP)
    {
        uint8_t ubVACode = (ACMP0->HYSTERESIS1 & _ACMP_HYSTERESIS1_DIVVA_MASK) >> _ACMP_HYSTERESIS1_DIVVA_SHIFT;
        uint8_t ubVBCode = (ACMP0->HYSTERESIS1 & _ACMP_HYSTERESIS1_DIVVB_MASK) >> _ACMP_HYSTERESIS1_DIVVB_SHIFT;

        *pfLTP = interp_rf_out_power(2500.f * (float)(ubVBCode + 1) / (float)(ubVACode + 1), pfRFPowerMeterPData, pfRFPowerMeterVData, ulRFPowerMeterDataSize);
    }
}

float ext_adc_get_5v0_current(uint32_t ulSamples)
{
    float fShuntVoltage = 0.f;

    for(uint32_t i = 0; i < ulSamples; i++)
        fShuntVoltage += mcp3421_read_adc(MCP3421_PGA_X1);

    fShuntVoltage /= ulSamples; // Average
    fShuntVoltage /= 20; // Differential amplifier gain

    return fShuntVoltage / 0.03f; // 0.03 Ohm current shunt resistor
}

float interp_rf_out_power(float fVoltage, const float *pfPData, const float *pfVData, uint32_t ulDataSize)
{
    fVoltage /= 1 + (2000 / 1000); // Op Amp gain

    // Interpolate
    float fV0;
    float fP0;
    float fV1;
    float fP1;

    if(fVoltage < pfVData[0])
    {
        fV0 = pfVData[0];
        fP0 = pfPData[0];
        fV1 = pfVData[1];
        fP1 = pfPData[1];
    }
    else if(fVoltage > pfVData[ulDataSize - 1])
    {
        fV0 = pfVData[ulDataSize - 2];
        fP0 = pfPData[ulDataSize - 2];
        fV1 = pfVData[ulDataSize - 1];
        fP1 = pfPData[ulDataSize - 1];
    }
    else
    {
        for(uint8_t i = 1; i < ulDataSize; i++)
        {
            fV0 = pfVData[i - 1];
            fP0 = pfPData[i - 1];
            fV1 = pfVData[i];
            fP1 = pfPData[i];

            if(fV0 <= fVoltage && fV1 > fVoltage)
                break;
        }
    }

    float fDeltaV = fV1 - fV0;
    float fDeltaP = fP1 - fP0;
    float fSlope = fDeltaP / fDeltaV;
    float fInterp = fP0 + (fVoltage - fV0) * fSlope;

    return fInterp;
}
float interp_rf_out_power_inv(float fPower, const float *pfPData, const float *pfVData, uint32_t ulDataSize)
{
    // Interpolate
    float fV0;
    float fP0;
    float fV1;
    float fP1;

    if(fPower < pfPData[0])
    {
        fV0 = pfVData[0];
        fP0 = pfPData[0];
        fV1 = pfVData[1];
        fP1 = pfPData[1];
    }
    else if(fPower > pfPData[ulDataSize - 1])
    {
        fV0 = pfVData[ulDataSize - 2];
        fP0 = pfPData[ulDataSize - 2];
        fV1 = pfVData[ulDataSize - 1];
        fP1 = pfPData[ulDataSize - 1];
    }
    else
    {
        for(uint8_t i = 1; i < ulDataSize; i++)
        {
            fV0 = pfVData[i - 1];
            fP0 = pfPData[i - 1];
            fV1 = pfVData[i];
            fP1 = pfPData[i];

            if(fP0 <= fPower && fP1 > fPower)
                break;
        }
    }

    float fDeltaV = fV1 - fV0;
    float fDeltaP = fP1 - fP0;
    float fSlope = fDeltaV / fDeltaP;
    float fInterp = fV0 + (fPower - fP0) * fSlope;

    fInterp *= 1 + (2000 / 1000); // Op Amp gain

    return fInterp;
}
float get_rf_out_power(uint32_t ulSamples, const float *pfPData, const float *pfVData, uint32_t ulDataSize)
{
    float fVoltage = 0.f;

    for(uint32_t i = 0; i < ulSamples; i++)
        fVoltage += adc_get_vpdet();

    fVoltage /= ulSamples; // Average

    return interp_rf_out_power(fVoltage, pfPData, pfVData, ulDataSize);
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

    usart0_init(12000000, 0, USART_SPI_MSB_FIRST, -1, 0, 0);
    usart1_init(1000000, USART_FRAME_STOPBITS_ONE | USART_FRAME_PARITY_NONE | USART_FRAME_DATABITS_EIGHT, -1, 5, -1, -1);

    i2c0_init(I2C_SLAVE_ADDRESS, 4, 4);
    i2c0_set_slave_addr_isr(i2c_slave_addr_isr);
    i2c0_set_slave_tx_data_isr(i2c_slave_tx_data_isr);
    i2c0_set_slave_rx_data_isr(i2c_slave_rx_data_isr);
    i2c1_init(I2C_FAST, 3, 3);

    char szDeviceCoreName[32];
    char szDeviceName[32];

    get_device_core_name(szDeviceCoreName, 32);
    get_device_name(szDeviceName, 32);

    printf("\x1B[2J"); // Clear the screen
    printf("\x1B[H"); // Move cursor to top left corner

    DBGPRINTLN_CTX("IcyRadio Upconverter v%lu (%s %s)!", BUILD_VERSION, __DATE__, __TIME__);
    DBGPRINTLN_CTX("Core: %s", szDeviceCoreName);
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

    if(mcp3421_init())
        DBGPRINTLN_CTX("MCP3421 init OK!");
    else
        DBGPRINTLN_CTX("MCP3421 init NOK!");

    if(adf4351_init())
        DBGPRINTLN_CTX("ADF4351 init OK!");
    else
        DBGPRINTLN_CTX("ADF4351 init NOK!");

    if(f1958_init(F1958_IF_ATT_ID))
        DBGPRINTLN_CTX("IF F1958 init OK!");
    else
        DBGPRINTLN_CTX("IF F1958 init NOK!");

    if(f1958_init(F1958_RF1_ATT_ID))
        DBGPRINTLN_CTX("RF1 F1958 init OK!");
    else
        DBGPRINTLN_CTX("RF1 F1958 init NOK!");

    if(f1958_init(F1958_RF2_ATT_ID))
        DBGPRINTLN_CTX("RF2 F1958 init OK!");
    else
        DBGPRINTLN_CTX("RF2 F1958 init NOK!");

    return 0;
}
int main()
{
    // Analog comparator to monitor RF Power
    acmp_init();

    // I2C Slave Register block
    i2c_slave_register_init();

    // 5V0 Current ADC
    mcp3421_write_config(MCP3421_RESOLUTION_16BIT | MCP3421_ONE_SHOT);

    // Attenuators
    f1958_set_attenuation(F1958_IF_ATT_ID, 32.75f);
    DBGPRINTLN_CTX("IF Attenuator value: -%.3f dB", (float)F1958_ATTENUATION[F1958_IF_ATT_ID]);
    f1958_set_attenuation(F1958_RF1_ATT_ID, 32.75f);
    DBGPRINTLN_CTX("RF Attenuator value: -%.3f dB", (float)F1958_ATTENUATION[F1958_RF1_ATT_ID]);
    f1958_set_attenuation(F1958_RF2_ATT_ID, 32.75f);
    DBGPRINTLN_CTX("RF Attenuator value: -%.3f dB", (float)F1958_ATTENUATION[F1958_RF2_ATT_ID]);

    // PLL
    adf4351_pfd_config(26000000, 1, 0, 13, 0);
    DBGPRINTLN_CTX("PLL Reference frequency: %.3f MHz", (float)ADF4351_REF_FREQ / 1000000);
    DBGPRINTLN_CTX("PLL PFD frequency: %.3f MHz", (float)ADF4351_PFD_FREQ / 1000000);

    adf4351_charge_pump_set_current(5.f); // 5 mA
    DBGPRINTLN_CTX("PLL CP current: %.2f mA", adf4351_charge_pump_get_current());

    adf4351_main_out_config(1, -1); // -1 dBm
    DBGPRINTLN_CTX("PLL output power: %i dBm", adf4351_main_out_get_power());

    adf4351_set_frequency(1875000000U);
    DBGPRINTLN_CTX("PLL output frequency: %.3f MHz", (float)ADF4351_FREQ / 1000000);

    //while(!PLL_LOCKED());
    //PLL_UNMUTE();
    //delay_ms(10);
    //MIXER_ENABLE();
    //delay_ms(100);
    //PA_STG3_ENABLE();
    //delay_ms(50);
    //PA_STG1_2_ENABLE();

    while(1)
    {
        wdog_feed();

        if(I2C_SLAVE_REGISTER(uint8_t, I2C_SLAVE_REGISTER_CONFIG) & BIT(7))
        {
            delay_ms(20);

            reset();
        }

        volatile uint8_t ubStatus = 0x00;
        volatile uint8_t ubConfig = 0x00;

        ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
            ubConfig = I2C_SLAVE_REGISTER(uint8_t, I2C_SLAVE_REGISTER_CONFIG);

        if(ubConfig & BIT(0))
            PLL_MUTE();
        else
            PLL_UNMUTE();
        if(ubConfig & BIT(1))
            MIXER_ENABLE();
        else
            MIXER_DISABLE();
        if(ubConfig & BIT(2))
            PA_STG1_2_ENABLE();
        else
            PA_STG1_2_DISABLE();
        if(ubConfig & BIT(3))
            PA_STG3_ENABLE();
        else
            PA_STG3_DISABLE();

        if(PLL_MUTED())
            ubStatus |= BIT(0);
        if(MIXER_STATUS())
            ubStatus |= BIT(1);
        if(PA_STG1_2_STATUS())
            ubStatus |= BIT(2);
        if(PA_STG3_STATUS())
            ubStatus |= BIT(3);
        if(PLL_LOCKED())
            ubStatus |= BIT(4);

        ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
            I2C_SLAVE_REGISTER(uint8_t, I2C_SLAVE_REGISTER_STATUS) = ubStatus;

        if(ublLOChanged)
        {
            ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
            {
                uint64_t ullLOFreq = I2C_SLAVE_REGISTER(uint64_t, I2C_SLAVE_REGISTER_LO_FREQ);

                if(PLL_MUTED())
                    adf4351_set_frequency(ullLOFreq);

                I2C_SLAVE_REGISTER(uint64_t, I2C_SLAVE_REGISTER_LO_FREQ) = ADF4351_FREQ;
                //I2C_SLAVE_REGISTER(uint32_t, I2C_SLAVE_REGISTER_LO_REF_FREQ) = ADF4351_REF_FREQ;
                //I2C_SLAVE_REGISTER(uint32_t, I2C_SLAVE_REGISTER_LO_PFD_FREQ) = ADF4351_PFD_FREQ;

                ublLOChanged = 0;
            }
        }

        if(ubAttChanged)
        {
            for(uint8_t i = 0; i < 3; i++)
            {
                ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
                {
                    if(!(ubAttChanged & BIT(i)))
                        continue;

                    f1958_set_attenuation(i, I2C_SLAVE_REGISTER(float, I2C_SLAVE_REGISTER_IF_ATT + i * sizeof(float)));
                    I2C_SLAVE_REGISTER(float, I2C_SLAVE_REGISTER_IF_ATT + i * sizeof(float)) = F1958_ATTENUATION[i];

                    ubAttChanged &= ~BIT(i);
                }
            }
        }

        if(ubRFPowerMeterChanged)
        {
            ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
            {
                uint8_t ubConfig = I2C_SLAVE_REGISTER(uint8_t, I2C_SLAVE_REGISTER_RF_OUT_PWR_CONFIG);
                float fLTP = I2C_SLAVE_REGISTER(float, I2C_SLAVE_REGISTER_RF_OUT_PWR_LOW_THRESH);

                const float * pfRFPowerMeterPDataNew = NULL;
                const float * pfRFPowerMeterVDataNew = NULL;
                uint32_t ulRFPowerMeterDataSizeNew = 0;

                switch(ubConfig & 0x0F)
                {
                    case 0:
                    {
                        pfRFPowerMeterPDataNew = fPData_CW;
                        pfRFPowerMeterVDataNew = fVData_CW;
                        ulRFPowerMeterDataSizeNew = sizeof(fPData_CW) / sizeof(float);
                    }
                    break;
                    case 1:
                    {
                        pfRFPowerMeterPDataNew = fPData_QPSK;
                        pfRFPowerMeterVDataNew = fVData_QPSK;
                        ulRFPowerMeterDataSizeNew = sizeof(fPData_QPSK) / sizeof(float);
                    }
                    break;
                    case 2:
                    {
                        pfRFPowerMeterPDataNew = fPData_8PSK;
                        pfRFPowerMeterVDataNew = fVData_8PSK;
                        ulRFPowerMeterDataSizeNew = sizeof(fPData_8PSK) / sizeof(float);
                    }
                    break;
                    case 3:
                    {
                        pfRFPowerMeterPDataNew = fPData_16APSK;
                        pfRFPowerMeterVDataNew = fVData_16APSK;
                        ulRFPowerMeterDataSizeNew = sizeof(fPData_16APSK) / sizeof(float);
                    }
                    break;
                    case 4:
                    {
                        pfRFPowerMeterPDataNew = fPData_32APSK;
                        pfRFPowerMeterVDataNew = fVData_32APSK;
                        ulRFPowerMeterDataSizeNew = sizeof(fPData_32APSK) / sizeof(float);
                    }
                    break;
                }

                float fOldUTP, fOldLTP;

                acmp_get_rf_pwr_thresh(&fOldUTP, &fOldLTP);

                if(pfRFPowerMeterPDataNew == NULL)
                {
                    if(pfRFPowerMeterPData == fPData_CW)
                        ubConfig = (ubConfig & ~0x0F) | 0x00;
                    else if(pfRFPowerMeterPData == fPData_QPSK)
                        ubConfig = (ubConfig & ~0x0F) | 0x01;
                    else if(pfRFPowerMeterPData == fPData_8PSK)
                        ubConfig = (ubConfig & ~0x0F) | 0x02;
                    else if(pfRFPowerMeterPData == fPData_16APSK)
                        ubConfig = (ubConfig & ~0x0F) | 0x03;
                    else if(pfRFPowerMeterPData == fPData_32APSK)
                        ubConfig = (ubConfig & ~0x0F) | 0x04;

                    I2C_SLAVE_REGISTER(uint8_t, I2C_SLAVE_REGISTER_RF_OUT_PWR_CONFIG) = ubConfig;
                }
                else if(pfRFPowerMeterPData != pfRFPowerMeterPDataNew)
                {
                    pfRFPowerMeterPData = pfRFPowerMeterPDataNew;
                    pfRFPowerMeterVData = pfRFPowerMeterVDataNew;
                    ulRFPowerMeterDataSize = ulRFPowerMeterDataSizeNew;
                }

                if(fOldLTP != fLTP)
                {
                    fOldLTP = fLTP;
                    fOldUTP = fLTP + 3.f;
                }

                acmp_set_rf_pwr_thresh(fOldUTP, fOldLTP);
                acmp_get_rf_pwr_thresh(NULL, &fLTP);

                I2C_SLAVE_REGISTER(float, I2C_SLAVE_REGISTER_RF_OUT_PWR_LOW_THRESH) = fLTP;

                ubRFPowerMeterChanged = 0;
            }
        }

        static uint64_t ullLastHeartBeat = 0;
        static uint64_t ullLastTelemetryUpdate = 0;

        if((g_ullSystemTick > 0 && ullLastHeartBeat == 0) || g_ullSystemTick - ullLastHeartBeat > 2000)
        {
            ullLastHeartBeat = g_ullSystemTick;

            LED_TOGGLE();

            if(LED_STATUS())
                ullLastHeartBeat -= 1900;
        }

        if((g_ullSystemTick > 0 && ullLastTelemetryUpdate == 0) || g_ullSystemTick - ullLastTelemetryUpdate > 5000)
        {
            ullLastTelemetryUpdate = g_ullSystemTick;

            // System Temperatures
            float fADCTemp = adc_get_temperature();
            float fEMUTemp = emu_get_temperature();

            ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
            {
                I2C_SLAVE_REGISTER(float, I2C_SLAVE_REGISTER_ADC_TEMP) = fADCTemp;
                I2C_SLAVE_REGISTER(float, I2C_SLAVE_REGISTER_EMU_TEMP) = fEMUTemp;
            }

            DBGPRINTLN_CTX("----------------------------------");
            DBGPRINTLN_CTX("ADC Temperature: %.2f C", fADCTemp);
            DBGPRINTLN_CTX("EMU Temperature: %.2f C", fEMUTemp);

            // System Voltages/Currents
            float fVIN = adc_get_vin();
            float f5V0 = adc_get_5v0();
            float f5V0I = ext_adc_get_5v0_current(10);
            float fAVDD = adc_get_avdd();
            float fDVDD = adc_get_dvdd();
            float fIOVDD = adc_get_iovdd();
            float fCoreVDD = adc_get_corevdd();

            ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
            {
                I2C_SLAVE_REGISTER(float, I2C_SLAVE_REGISTER_VIN_VOLTAGE) = fVIN;
                I2C_SLAVE_REGISTER(float, I2C_SLAVE_REGISTER_5V0_VOLTAGE) = f5V0;
                I2C_SLAVE_REGISTER(float, I2C_SLAVE_REGISTER_5V0_CURRENT) = f5V0I;
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
            DBGPRINTLN_CTX("5V0 Current: %.2f mA", f5V0I);

            // Output Power
            float fOutputPower = get_rf_out_power(10, pfRFPowerMeterPData, pfRFPowerMeterVData, ulRFPowerMeterDataSize);

            ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
            {
                I2C_SLAVE_REGISTER(float, I2C_SLAVE_REGISTER_RF_OUT_PWR) = fOutputPower;
            }

            DBGPRINTLN_CTX("----------------------------------");
            DBGPRINTLN_CTX("RF Output Power: %.2f dBm", fOutputPower);

            float fUTP, fLTP;
            acmp_get_rf_pwr_thresh(&fUTP, &fLTP);

            DBGPRINTLN_CTX("RF Output Power low threshold: %.2f dBm", fLTP);
            DBGPRINTLN_CTX("RF Output Power high threshold: %.2f dBm", fUTP);
            DBGPRINTLN_CTX("RF Output Power status: %s", (ACMP0->STATUS & ACMP_STATUS_ACMPOUT) ? "HIGH" : "LOW");

            DBGPRINTLN_CTX("----------------------------------");
            DBGPRINTLN_CTX("LO Frequency: %llu Hz", ADF4351_FREQ);
        }
    }

    return 0;
}