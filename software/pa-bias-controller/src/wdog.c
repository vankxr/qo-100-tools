#include "wdog.h"

wdog_warning_isr_t pfWarningISR = NULL;

void _wdog0_isr()
{
    uint32_t ulFlags = WDOG0->IFC;

    if(ulFlags & WDOG_IFC_WARN)
        if(pfWarningISR)
            pfWarningISR();
}

void wdog_init(uint32_t ulTimeoutConfig)
{
    ulTimeoutConfig &= _WDOG_CTRL_WARNSEL_MASK | _WDOG_CTRL_PERSEL_MASK;

    cmu_hfbus_clock_gate(CMU_HFBUSCLKEN0_LE, 1);

    cmu_update_clocks();

    WDOG0->CTRL = WDOG_CTRL_WDOGRSTDIS_EN | WDOG_CTRL_CLRSRC_SW | ulTimeoutConfig | WDOG_CTRL_CLKSEL_ULFRCO | WDOG_CTRL_SWOSCBLOCK | WDOG_CTRL_EM4BLOCK | WDOG_CTRL_EN;
    WDOG0->CMD = WDOG_CMD_CLEAR;

    WDOG0->IFC = _WDOG_IFC_MASK; // Clear all flags
    IRQ_CLEAR(WDOG0_IRQn); // Clear pending vector
    IRQ_SET_PRIO(WDOG0_IRQn, 0, 0); // Set priority 0,0
    IRQ_ENABLE(WDOG0_IRQn); // Enable vector
    WDOG0->IEN |= WDOG_IEN_WARN; // Enable WARN flag

    WDOG0->CTRL |= WDOG_CTRL_LOCK;
}
void wdog_set_warning_isr(wdog_warning_isr_t pfISR)
{
    pfWarningISR = pfISR;
}
float wdog_get_timeout_period()
{
    uint8_t ubPeriod = (WDOG0->CTRL & _WDOG_CTRL_PERSEL_MASK) >> _WDOG_CTRL_PERSEL_SHIFT;

    return (float)((1 << (ubPeriod + 3)) + 1.f) / ULFRCO_OSC_FREQ * 1000.f;
}
float wdog_get_warning_period()
{
    uint8_t ubPeriod = (WDOG0->CTRL & _WDOG_CTRL_WARNSEL_MASK) >> _WDOG_CTRL_WARNSEL_SHIFT;

    return wdog_get_timeout_period() * 0.25f * ubPeriod;
}
void wdog_feed()
{
    WDOG0->CMD = WDOG_CMD_CLEAR;
}