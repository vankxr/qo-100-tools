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
#include "dbg.h"
#include "msc.h"
#include "rtcc.h"
#include "adc.h"
#include "crc.h"
#include "usart.h"
#include "adf4351.h"
#include "f1951.h"

// Structs

// Forward declarations
static void reset() __attribute__((noreturn));
static void sleep();

static uint32_t get_free_ram();

void get_device_name(char *pszDeviceName, uint32_t ulDeviceNameSize);
static uint16_t get_device_revision();

// Variables

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

int init()
{
    rmu_init(RMU_CTRL_PINRMODE_FULL, RMU_CTRL_SYSRMODE_EXTENDED, RMU_CTRL_LOCKUPRMODE_EXTENDED, RMU_CTRL_WDOGRMODE_EXTENDED); // Init RMU and set reset modes

    emu_init(0); // Init EMU
    emu_dcdc_init(1800.f, 50.f, 100.f, 0.f); // Init DC-DC converter (1.8 V, 50 mA active, 100 uA sleep, 0 mA reverse limit)

    cmu_init(); // Init Clocks

    dbg_init(); // Init Debug module
    dbg_swo_config(BIT(0) | BIT(1), 2000000); // Init SWO channels 0 and 1 at 2 MHz

    msc_init(); // Init Flash, RAM and caches

    systick_init(); // Init system tick

    gpio_init(); // Init GPIOs
    ldma_init(); // Init LDMA
    rtcc_init(); // Init RTCC
    crc_init(); // Init CRC calculation unit
    adc_init(); // Init ADCs

    float fAVDDHighThresh, fAVDDLowThresh;
    float fDVDDHighThresh, fDVDDLowThresh;
    float fIOVDDHighThresh, fIOVDDLowThresh;

    emu_vmon_avdd_config(1, 3.1f, &fAVDDLowThresh, 3.22f, &fAVDDHighThresh); // Enable AVDD monitor
    emu_vmon_dvdd_config(1, 1.7f, &fDVDDLowThresh); // Enable DVDD monitor
    emu_vmon_iovdd_config(1, 3.15f, &fIOVDDLowThresh); // Enable IOVDD monitor

    fDVDDHighThresh = fDVDDLowThresh + 0.026f; // Hysteresis from datasheet
    fIOVDDHighThresh = fIOVDDLowThresh + 0.026f; // Hysteresis from datasheet

    usart0_init(16000000, 0, USART_SPI_MSB_FIRST, -1, 19, 18);

    char szDeviceName[32];

    get_device_name(szDeviceName, 32);

    DBGPRINTLN_CTX("IcyRadio Upconverter v%lu (%s %s)!", BUILD_VERSION, __DATE__, __TIME__);
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
    DBGPRINTLN_CTX("EMU - 5V0 Voltage: %.2f mV", adc_get_5v0());
    DBGPRINTLN_CTX("EMU - 12V0 Voltage: %.2f mV", adc_get_12v0());
    DBGPRINTLN_CTX("EMU - VIN Voltage: %.2f mV", adc_get_vin());

    DBGPRINTLN_CTX("CMU - HFXO Oscillator: %.3f MHz", (float)HFXO_OSC_FREQ / 1000000);
    DBGPRINTLN_CTX("CMU - HFRCO Oscillator: %.3f MHz", (float)HFRCO_OSC_FREQ / 1000000);
    DBGPRINTLN_CTX("CMU - AUXHFRCO Oscillator: %.3f MHz", (float)AUXHFRCO_OSC_FREQ / 1000000);
    DBGPRINTLN_CTX("CMU - LFXO Oscillator: %.3f kHz", (float)LFXO_OSC_FREQ / 1000);
    DBGPRINTLN_CTX("CMU - LFRCO Oscillator: %.3f kHz", (float)LFRCO_OSC_FREQ / 1000);
    DBGPRINTLN_CTX("CMU - ULFRCO Oscillator: %.3f kHz", (float)ULFRCO_OSC_FREQ / 1000);
    DBGPRINTLN_CTX("CMU - HFSRC Clock: %.3f MHz", (float)HFSRC_CLOCK_FREQ / 1000000);
    DBGPRINTLN_CTX("CMU - HF Clock: %.3f MHz", (float)HF_CLOCK_FREQ / 1000000);
    DBGPRINTLN_CTX("CMU - HFCORE Clock: %.3f MHz", (float)HFCORE_CLOCK_FREQ / 1000000);
    DBGPRINTLN_CTX("CMU - HFEXP Clock: %.3f MHz", (float)HFEXP_CLOCK_FREQ / 1000000);
    DBGPRINTLN_CTX("CMU - HFPER Clock: %.3f MHz", (float)HFPER_CLOCK_FREQ / 1000000);
    DBGPRINTLN_CTX("CMU - HFLE Clock: %.3f MHz", (float)HFLE_CLOCK_FREQ / 1000000);
    DBGPRINTLN_CTX("CMU - ADC0 Clock: %.3f MHz", (float)ADC0_CLOCK_FREQ / 1000000);
    DBGPRINTLN_CTX("CMU - DBG Clock: %.3f MHz", (float)DBG_CLOCK_FREQ / 1000000);
    DBGPRINTLN_CTX("CMU - AUX Clock: %.3f MHz", (float)AUX_CLOCK_FREQ / 1000000);
    DBGPRINTLN_CTX("CMU - LFA Clock: %.3f kHz", (float)LFA_CLOCK_FREQ / 1000);
    DBGPRINTLN_CTX("CMU - LETIMER0 Clock: %.3f kHz", (float)LETIMER0_CLOCK_FREQ / 1000);
    DBGPRINTLN_CTX("CMU - LFB Clock: %.3f kHz", (float)LFB_CLOCK_FREQ / 1000);
    DBGPRINTLN_CTX("CMU - LEUART0 Clock: %.3f kHz", (float)LEUART0_CLOCK_FREQ / 1000);
    DBGPRINTLN_CTX("CMU - LFE Clock: %.3f kHz", (float)LFE_CLOCK_FREQ / 1000);
    DBGPRINTLN_CTX("CMU - RTCC Clock: %.3f kHz", (float)RTCC_CLOCK_FREQ / 1000);

    DBGPRINTLN_CTX("Waiting 1000 ms...");

    delay_ms(1000);

    if(adf4351_init())
        DBGPRINTLN_CTX("ADF4351 init OK!");
    else
        DBGPRINTLN_CTX("ADF4351 init NOK!");

    if(f1951_init())
        DBGPRINTLN_CTX("F1951 init OK!");
    else
        DBGPRINTLN_CTX("F1951 init NOK!");

    return 0;
}
int main()
{
    // Attenuator
    f1951_set_attenuation(32.5f);
    DBGPRINTLN_CTX("Attenuator value: -%.3f dB", (float)F1951_ATTENUATION);

    // PLL
    adf4351_pfd_config(26000000, 1, 0, 50, 0);
    DBGPRINTLN_CTX("PLL Reference frequency: %.3f MHz", (float)ADF4351_REF_FREQ / 1000000);
    DBGPRINTLN_CTX("PLL PFD frequency: %.3f MHz", (float)ADF4351_PFD_FREQ / 1000000);

    adf4351_charge_pump_set_current(2.f); // 2 mA
    DBGPRINTLN_CTX("PLL CP current: %.2f mA", adf4351_charge_pump_get_current());

    adf4351_main_out_config(1, -4); // -4 dBm
    DBGPRINTLN_CTX("PLL output power: %i dBm", adf4351_main_out_get_power());

    adf4351_set_frequency(1975000000U);
    DBGPRINTLN_CTX("PLL output frequency: %.3f MHz", (float)ADF4351_FREQ / 1000000);

    PLL_UNMUTE();

    MIXER_ENABLE();

    delay_ms(200);

    PA_5V0_ENABLE();
    //PA_12V0_ENABLE();

    while(1)
    {
        static uint64_t ullLastLEDTick = 0;
        static uint64_t ullLastDebugPrint = 0;

        if(g_ullSystemTick - ullLastLEDTick > 500)
        {
            ullLastLEDTick = g_ullSystemTick;

            LED_TOGGLE();
        }

        if(g_ullSystemTick - ullLastDebugPrint > 2000)
        {
            ullLastDebugPrint = g_ullSystemTick;

            DBGPRINTLN_CTX("----------------------------------");
            DBGPRINTLN_CTX("ADC temperature: %.2f C", adc_get_temperature());
            DBGPRINTLN_CTX("EMU temperature: %.2f C", emu_get_temperature());
            DBGPRINTLN_CTX("AVDD Voltage: %.2f mV", adc_get_avdd());
            DBGPRINTLN_CTX("DVDD Voltage: %.2f mV", adc_get_dvdd());
            DBGPRINTLN_CTX("IOVDD Voltage: %.2f mV", adc_get_iovdd());
            DBGPRINTLN_CTX("Core Voltage: %.2f mV", adc_get_corevdd());
            DBGPRINTLN_CTX("5V0 Voltage: %.2f mV", adc_get_5v0());
            DBGPRINTLN_CTX("12V0 Voltage: %.2f mV", adc_get_12v0());
            DBGPRINTLN_CTX("VIN Voltage: %.2f mV", adc_get_vin());
        }
    }

    return 0;
}