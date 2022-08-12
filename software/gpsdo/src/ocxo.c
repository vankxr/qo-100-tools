#include "ocxo.h"
#include "debug_macros.h" // TODO: Remove

static uint16_t usControlVoltageWord = 0;
static ocxo_tick_count_done_callback_t pfTickCountDoneCallback = NULL;
static uint16_t usTotalTicks = 0;
static volatile uint16_t usCurrentTicks = 0;
static volatile uint8_t ubCountStatus = 0;
static volatile uint8_t ubTickStatus = 0;
static volatile uint64_t ullLastTick = 0;
static volatile uint64_t ullLastPowerup = 0;

void _wtimer0_isr()
{
    uint32_t ulT0Flags = WTIMER0->IF;
    uint32_t ulT1Flags = WTIMER1->IF;

    if(ulT0Flags & WTIMER_IF_CC0)
    {
        while(!(ulT1Flags & WTIMER_IF_CC0))
            ulT1Flags = WTIMER1->IF;

        if(usCurrentTicks >= usTotalTicks)
        {
            uint64_t ullCapture = ((uint64_t)(WTIMER1->CC[0].CCV) << 32) | WTIMER0->CC[0].CCV;

            if(ullCapture > 0)
            {
                PRS->CH[1].CTRL = PRS_CH_CTRL_ASYNC | PRS_CH_CTRL_EDSEL_OFF | PRS_CH_CTRL_SOURCESEL_NONE;
                WTIMER0->CMD = WTIMER_CMD_STOP;
                WTIMER1->CMD = WTIMER_CMD_STOP;

                if(pfTickCountDoneCallback != NULL)
                {
                    pfTickCountDoneCallback(usCurrentTicks, ullCapture);

                    pfTickCountDoneCallback = NULL;
                }

                //DBGPRINTLN_CTX("Captured CC0: %llu", ullCapture);
            }
        }
        else
        {
            REG_DISCARD(&(WTIMER0->CC[0].CCV));
            REG_DISCARD(&(WTIMER1->CC[0].CCV));
        }

        WTIMER1->IFC = ulT1Flags;
    }

    WTIMER0->IFC = ulT0Flags;
}

uint8_t ocxo_init()
{
    ocxo_power_up();

    // Cascade two WTIMERs to measure the OCXO referenced to the GPS timepulse with a 64-bit resolution
    cmu_hfbus_clock_gate(CMU_HFBUSCLKEN0_PRS, 1);

    PRS->CH[0].CTRL = PRS_CH_CTRL_ASYNC | PRS_CH_CTRL_EDSEL_OFF | PRS_CH_CTRL_SOURCESEL_GPIOH | PRS_CH_CTRL_SIGSEL_GPIOPIN12; // OCXO Clock
    PRS->CH[1].CTRL = PRS_CH_CTRL_ASYNC | PRS_CH_CTRL_EDSEL_OFF | PRS_CH_CTRL_SOURCESEL_GPIOH | PRS_CH_CTRL_SIGSEL_GPIOPIN13; // GPS Timepulse

    cmu_hfper1_clock_gate(CMU_HFPERCLKEN1_WTIMER0, 1);
    WTIMER0->CTRL = WTIMER_CTRL_CLKSEL_CC1 | WTIMER_CTRL_FALLA_NONE | WTIMER_CTRL_RISEA_START | WTIMER_CTRL_MODE_UP;
    WTIMER0->TOP = 0xFFFFFFFF;
    WTIMER0->CNT = 0x00000000;
    WTIMER0->CC[0].CTRL = WTIMER_CC_CTRL_INSEL_PRS | WTIMER_CC_CTRL_ICEVCTRL_EVERYEDGE | WTIMER_CC_CTRL_ICEDGE_RISING | WTIMER_CC_CTRL_PRSSEL_PRSCH1 | WTIMER_CC_CTRL_MODE_INPUTCAPTURE;
    WTIMER0->CC[1].CTRL = WTIMER_CC_CTRL_INSEL_PRS | WTIMER_CC_CTRL_ICEDGE_RISING | WTIMER_CC_CTRL_PRSSEL_PRSCH0 | WTIMER_CC_CTRL_MODE_OFF;

    WTIMER0->IFC = _WTIMER_IFC_MASK; // Clear pending IRQs
    IRQ_CLEAR(WTIMER0_IRQn); // Clear pending vector
    IRQ_SET_PRIO(WTIMER0_IRQn, 1, 0); // Set priority 1,0
    IRQ_ENABLE(WTIMER0_IRQn); // Enable vector
    WTIMER0->IEN = WTIMER_IEN_CC0; // Enable interrupts

    cmu_hfper1_clock_gate(CMU_HFPERCLKEN1_WTIMER1, 1);
    WTIMER1->CTRL = WTIMER_CTRL_CLKSEL_TIMEROUF | WTIMER_CTRL_FALLA_NONE | WTIMER_CTRL_RISEA_START | WTIMER_CTRL_MODE_UP;
    WTIMER1->TOP = 0xFFFFFFFF;
    WTIMER1->CNT = 0x00000000;
    WTIMER1->CC[0].CTRL = WTIMER_CC_CTRL_INSEL_PRS | WTIMER_CC_CTRL_ICEVCTRL_EVERYEDGE | WTIMER_CC_CTRL_ICEDGE_RISING | WTIMER_CC_CTRL_PRSSEL_PRSCH1 | WTIMER_CC_CTRL_MODE_INPUTCAPTURE;

    return 1;
}
void gps_timepulse_isr()
{
    ubTickStatus = GPS_TIMEPULSE();

    if(ubTickStatus)
        ullLastTick = g_ullSystemTick;

    if(!PWR_OCXO_STATUS()) // Skip if not powered
        return;

    if(!ubCountStatus)
        return;

    if(GPS_TIMEPULSE())
    {
        if(usCurrentTicks < usTotalTicks)
            usCurrentTicks++;
    }
    else
    {
        if(usCurrentTicks >= usTotalTicks)
        {
            WTIMER0->CC[0].CTRL = WTIMER_CC_CTRL_INSEL_PRS | WTIMER_CC_CTRL_ICEVCTRL_EVERYEDGE | WTIMER_CC_CTRL_ICEDGE_RISING | WTIMER_CC_CTRL_PRSSEL_PRSCH1 | WTIMER_CC_CTRL_MODE_INPUTCAPTURE;
            WTIMER1->CC[0].CTRL = WTIMER_CC_CTRL_INSEL_PRS | WTIMER_CC_CTRL_ICEVCTRL_EVERYEDGE | WTIMER_CC_CTRL_ICEDGE_RISING | WTIMER_CC_CTRL_PRSSEL_PRSCH1 | WTIMER_CC_CTRL_MODE_INPUTCAPTURE;
            PRS->CH[1].CTRL = PRS_CH_CTRL_ASYNC | PRS_CH_CTRL_EDSEL_OFF | PRS_CH_CTRL_SOURCESEL_GPIOH | PRS_CH_CTRL_SIGSEL_GPIOPIN13;

            ubCountStatus = 0;
        }
        else
        {
            PRS->CH[1].CTRL = PRS_CH_CTRL_ASYNC | PRS_CH_CTRL_EDSEL_OFF | PRS_CH_CTRL_SOURCESEL_NONE;
        }
    }
}

void ocxo_power_down()
{
    PWR_OCXO_DISABLE();
}
void ocxo_power_up()
{
    if(PWR_OCXO_STATUS())
        return;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        PWR_OCXO_ENABLE();

        ullLastPowerup = g_ullSystemTick;
    }

    delay_ms(50);

    ocxo_set_control_voltage_word(usControlVoltageWord);
}
uint64_t ocxo_get_warmup_time()
{
    if(!PWR_OCXO_STATUS())
        return 0;

    return g_ullSystemTick - ullLastPowerup;
}

void ocxo_set_control_voltage(float fVCont)
{
    const float fVMin = 0.f;
    const float fVMax = OCXO_CONTROL_VOLTAGE_REF * (65535.f / 65536.f);

    if(fVCont < fVMin)
        fVCont = fVMin;
    else if(fVCont > fVMax)
        fVCont = fVMax;

    ocxo_set_control_voltage_word(fVCont * (65536.f / OCXO_CONTROL_VOLTAGE_REF));
}
float ocxo_get_control_voltage()
{
    return ((float)usControlVoltageWord / 65536.f) * OCXO_CONTROL_VOLTAGE_REF;
}
void ocxo_set_control_voltage_word(uint16_t usVCont)
{
    usControlVoltageWord = usVCont;

    if(!PWR_OCXO_STATUS()) // DAC power is also gated through the PWR_OCXO_ENABLE() macro
        return;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        DAC_SELECT();

        usart0_spi_write_byte((usVCont >> 8) & 0xFF, 0);
        usart0_spi_write_byte((usVCont >> 0) & 0xFF, 1);

        DAC_UNSELECT();
    }
}
uint16_t ocxo_get_control_voltage_word()
{
    return usControlVoltageWord;
}

uint8_t ocxo_count_ticks(uint16_t usTicks, ocxo_tick_count_done_callback_t pfCallback)
{
    if(!PWR_OCXO_STATUS()) // Skip if not powered
        return OCXO_COUNT_NOT_POWERED;

    if(ubCountStatus)
        return OCXO_COUNT_ALREADY_RUNNING;

    if(!usTicks)
        return OCXO_COUNT_FAIL;

    if(!pfCallback)
        return OCXO_COUNT_FAIL;

    if(!ocxo_count_is_tick_present()) // Prevent infinite loop below if tick signal is not present
        return OCXO_COUNT_TICK_NOT_PRESENT;

    ubCountStatus = 0;

    // Ensure PPS has just gone low
    while(!ubTickStatus);
    while(ubTickStatus);

    usTotalTicks = usTicks;
    usCurrentTicks = 0;
    pfTickCountDoneCallback = pfCallback;

    WTIMER0->CNT = 0x00000000;
    WTIMER0->CC[0].CTRL = WTIMER_CC_CTRL_INSEL_PRS | WTIMER_CC_CTRL_ICEVCTRL_EVERYEDGE | WTIMER_CC_CTRL_ICEDGE_RISING | WTIMER_CC_CTRL_PRSSEL_PRSCH1 | WTIMER_CC_CTRL_MODE_OFF;
    while(WTIMER0->STATUS & WTIMER_STATUS_ICV0)
        REG_DISCARD(&(WTIMER0->CC[0].CCV));

    WTIMER1->CNT = 0x00000000;
    WTIMER1->CC[0].CTRL = WTIMER_CC_CTRL_INSEL_PRS | WTIMER_CC_CTRL_ICEVCTRL_EVERYEDGE | WTIMER_CC_CTRL_ICEDGE_RISING | WTIMER_CC_CTRL_PRSSEL_PRSCH1 | WTIMER_CC_CTRL_MODE_OFF;
    while(WTIMER1->STATUS & WTIMER_STATUS_ICV0)
        REG_DISCARD(&(WTIMER1->CC[0].CCV));

    ubCountStatus = 1;
    PRS->CH[1].CTRL = PRS_CH_CTRL_ASYNC | PRS_CH_CTRL_EDSEL_OFF | PRS_CH_CTRL_SOURCESEL_GPIOH | PRS_CH_CTRL_SIGSEL_GPIOPIN13;

    return OCXO_COUNT_OK;
}
uint8_t ocxo_count_is_running()
{
    return ubCountStatus;
}
uint16_t ocxo_count_get_ticks()
{
    return usCurrentTicks;
}
uint8_t ocxo_count_is_tick_present()
{
    return g_ullSystemTick - ullLastTick < 1500;
}
