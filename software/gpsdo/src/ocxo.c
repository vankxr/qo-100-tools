#include "ocxo.h"

uint8_t ocxo_init()
{
    PWR_OCXO_ENABLE();

    delay_ms(50);

    ocxo_set_vcont(2000.0); // Half scale

    // TODO: Cascade two WTIMERs to measure the OCXO referenced to the GPS timepulse with a 64-bit resolution

    return 1;
}
void gps_timepulse_isr()
{
    #include "debug_macros.h"
    DBGPRINTLN_CTX("PPS TICK");
}

void ocxo_set_vcont(float fVCont)
{
    if(!PWR_OCXO_STATUS()) // DAC power is also gated through the PWR_OCXO_ENABLE() macro
        return;

    const float fVRef = 4096.f;
    const float fVMin = 0.f;
    const float fVMax = fVRef * (65535.f / 65536.f);

    if(fVCont < fVMin)
        fVCont = fVMin;
    else if(fVCont > fVMax)
        fVCont = fVMax;

    uint16_t usVCont = fVCont * (65536.f / fVRef);

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        DAC_SELECT();

        usart0_spi_write_byte((usVCont >> 8) & 0xFF, 0);
        usart0_spi_write_byte((usVCont >> 0) & 0xFF, 1);

        DAC_UNSELECT();
    }
}

