#include "gpio.h"

static void gpio_isr(uint32_t ulFlags)
{

}
void _gpio_even_isr()
{
    uint32_t ulFlags = GPIO->IF;

    gpio_isr(ulFlags & 0x55555555);

    GPIO->IFC = 0x55555555; // Clear all even flags
}
void _gpio_odd_isr()
{
    uint32_t ulFlags = GPIO->IF;

    gpio_isr(ulFlags & 0xAAAAAAAA);

    GPIO->IFC = 0xAAAAAAAA; // Clear all odd flags
}

void gpio_init()
{
    cmu_hfbus_clock_gate(CMU_HFBUSCLKEN0_GPIO, 1);

    // NC - Not Connected (not available in mcu package)
    // NR - Not routed (no routing to pin on pcb, floating)
    // NU - Not used (not currently in use)

    // Port A
    GPIO->P[0].CTRL   = GPIO_P_CTRL_DRIVESTRENGTHALT_STRONG | (5 << _GPIO_P_CTRL_SLEWRATEALT_SHIFT)
                      | GPIO_P_CTRL_DRIVESTRENGTH_STRONG | (5 << _GPIO_P_CTRL_SLEWRATE_SHIFT);
    GPIO->P[0].MODEL  = GPIO_P_MODEL_MODE0_PUSHPULL     // PLL_LE
                      | GPIO_P_MODEL_MODE1_PUSHPULL     // nRF_ATT_CS
                      | GPIO_P_MODEL_MODE2_PUSHPULL     // nPA_STG1_BIAS_EN
                      | GPIO_P_MODEL_MODE3_DISABLED     // NC
                      | GPIO_P_MODEL_MODE4_DISABLED     // NC
                      | GPIO_P_MODEL_MODE5_DISABLED     // NC
                      | GPIO_P_MODEL_MODE6_DISABLED     // NC
                      | GPIO_P_MODEL_MODE7_DISABLED;    // NC
    GPIO->P[0].MODEH  = GPIO_P_MODEH_MODE8_DISABLED     // NC
                      | GPIO_P_MODEH_MODE9_DISABLED     // NC
                      | GPIO_P_MODEH_MODE10_DISABLED    // NC
                      | GPIO_P_MODEH_MODE11_DISABLED    // NC
                      | GPIO_P_MODEH_MODE12_DISABLED    // NC
                      | GPIO_P_MODEH_MODE13_DISABLED    // NC
                      | GPIO_P_MODEH_MODE14_DISABLED    // NC
                      | GPIO_P_MODEH_MODE15_DISABLED;   // NC
    GPIO->P[0].DOUT   = BIT(1) | BIT(2);
    GPIO->P[0].OVTDIS = 0;

    // Port B
    GPIO->P[1].CTRL   = GPIO_P_CTRL_DRIVESTRENGTHALT_STRONG | (5 << _GPIO_P_CTRL_SLEWRATEALT_SHIFT)
                      | GPIO_P_CTRL_DRIVESTRENGTH_STRONG | (5 << _GPIO_P_CTRL_SLEWRATE_SHIFT);
    GPIO->P[1].MODEL  = GPIO_P_MODEL_MODE0_DISABLED         // NC
                      | GPIO_P_MODEL_MODE1_DISABLED         // NC
                      | GPIO_P_MODEL_MODE2_DISABLED         // NC
                      | GPIO_P_MODEL_MODE3_DISABLED         // NC
                      | GPIO_P_MODEL_MODE4_DISABLED         // NC
                      | GPIO_P_MODEL_MODE5_DISABLED         // NC
                      | GPIO_P_MODEL_MODE6_DISABLED         // NC
                      | GPIO_P_MODEL_MODE7_PUSHPULL;        // LED
    GPIO->P[1].MODEH  = GPIO_P_MODEH_MODE8_DISABLED         // NU
                      | GPIO_P_MODEH_MODE9_DISABLED         // NC
                      | GPIO_P_MODEH_MODE10_DISABLED        // NC
                      | GPIO_P_MODEH_MODE11_DISABLED        // NU
                      | GPIO_P_MODEH_MODE12_DISABLED        // NC
                      | GPIO_P_MODEH_MODE13_DISABLED        // 5V0_VSENSE
                      | GPIO_P_MODEH_MODE14_DISABLED        // VIN_VSENSE
                      | GPIO_P_MODEH_MODE15_DISABLED;       // NC
    GPIO->P[1].DOUT   = 0;
    GPIO->P[1].OVTDIS = BIT(13) | BIT(14);

    // Port C
    GPIO->P[2].CTRL   = GPIO_P_CTRL_DRIVESTRENGTHALT_STRONG | (5 << _GPIO_P_CTRL_SLEWRATEALT_SHIFT)
                      | GPIO_P_CTRL_DRIVESTRENGTH_STRONG | (5 << _GPIO_P_CTRL_SLEWRATE_SHIFT);
    GPIO->P[2].MODEL  = GPIO_P_MODEL_MODE0_PUSHPULL         // PA_STG2_BIAS_EN
                      | GPIO_P_MODEL_MODE1_DISABLED         // NU
                      | GPIO_P_MODEL_MODE2_DISABLED         // NC
                      | GPIO_P_MODEL_MODE3_DISABLED         // NC
                      | GPIO_P_MODEL_MODE4_DISABLED         // NC
                      | GPIO_P_MODEL_MODE5_DISABLED         // NC
                      | GPIO_P_MODEL_MODE6_DISABLED         // NC
                      | GPIO_P_MODEL_MODE7_DISABLED;        // NC
    GPIO->P[2].MODEH  = GPIO_P_MODEH_MODE8_DISABLED         // NC
                      | GPIO_P_MODEH_MODE9_DISABLED         // NC
                      | GPIO_P_MODEH_MODE10_DISABLED        // NC
                      | GPIO_P_MODEH_MODE11_DISABLED        // NC
                      | GPIO_P_MODEH_MODE12_DISABLED        // NC
                      | GPIO_P_MODEH_MODE13_PUSHPULL        // nIF_ATT_CS
                      | GPIO_P_MODEH_MODE14_PUSHPULL        // nMIXER_SHDN
                      | GPIO_P_MODEH_MODE15_PUSHPULL;       // PLL_EN
    GPIO->P[2].DOUT   = BIT(13);
    GPIO->P[2].OVTDIS = 0;

    // Port D
    GPIO->P[3].CTRL   = GPIO_P_CTRL_DRIVESTRENGTHALT_STRONG | (5 << _GPIO_P_CTRL_SLEWRATEALT_SHIFT)
                      | GPIO_P_CTRL_DRIVESTRENGTH_STRONG | (5 << _GPIO_P_CTRL_SLEWRATE_SHIFT);
    GPIO->P[3].MODEL  = GPIO_P_MODEL_MODE0_DISABLED                 // NC
                      | GPIO_P_MODEL_MODE1_DISABLED                 // NC
                      | GPIO_P_MODEL_MODE2_DISABLED                 // NC
                      | GPIO_P_MODEL_MODE3_DISABLED                 // NC
                      | GPIO_P_MODEL_MODE4_WIREDANDPULLUPFILTER     // I2C1_SDA - Location 3
                      | GPIO_P_MODEL_MODE5_WIREDANDPULLUPFILTER     // I2C1_SCL - Location 3
                      | GPIO_P_MODEL_MODE6_WIREDANDPULLUPFILTER     // I2C0_SDA - Location 1
                      | GPIO_P_MODEL_MODE7_WIREDANDPULLUPFILTER;    // I2C0_SCL - Location 1
    GPIO->P[3].MODEH  = GPIO_P_MODEH_MODE8_DISABLED                 // NC
                      | GPIO_P_MODEH_MODE9_DISABLED                 // NC
                      | GPIO_P_MODEH_MODE10_DISABLED                // NC
                      | GPIO_P_MODEH_MODE11_DISABLED                // NC
                      | GPIO_P_MODEH_MODE12_DISABLED                // NC
                      | GPIO_P_MODEH_MODE13_DISABLED                // NC
                      | GPIO_P_MODEH_MODE14_DISABLED                // NC
                      | GPIO_P_MODEH_MODE15_DISABLED;               // NC
    GPIO->P[3].DOUT   = 0;
    GPIO->P[3].OVTDIS = 0;

    // Port E
    GPIO->P[4].CTRL   = GPIO_P_CTRL_DRIVESTRENGTHALT_STRONG | (5 << _GPIO_P_CTRL_SLEWRATEALT_SHIFT)
                      | GPIO_P_CTRL_DRIVESTRENGTH_STRONG | (5 << _GPIO_P_CTRL_SLEWRATE_SHIFT);
    GPIO->P[4].MODEL  = GPIO_P_MODEL_MODE0_DISABLED                 // NC
                      | GPIO_P_MODEL_MODE1_DISABLED                 // NC
                      | GPIO_P_MODEL_MODE2_DISABLED                 // NC
                      | GPIO_P_MODEL_MODE3_DISABLED                 // NC
                      | GPIO_P_MODEL_MODE4_DISABLED                 // NC
                      | GPIO_P_MODEL_MODE5_DISABLED                 // NC
                      | GPIO_P_MODEL_MODE6_DISABLED                 // NC
                      | GPIO_P_MODEL_MODE7_DISABLED;                // NC
    GPIO->P[4].MODEH  = GPIO_P_MODEH_MODE8_DISABLED                 // NC
                      | GPIO_P_MODEH_MODE9_DISABLED                 // NC
                      | GPIO_P_MODEH_MODE10_PUSHPULL                // SPI_MOSI (USART0_TX - Location 0)
                      | GPIO_P_MODEH_MODE11_INPUTPULLFILTER         // PLL_MUXOUT
                      | GPIO_P_MODEH_MODE12_PUSHPULL                // SPI_SCK (USART0_CLK - Location 0)
                      | GPIO_P_MODEH_MODE13_PUSHPULL                // nPLL_MUTE
                      | GPIO_P_MODEH_MODE14_DISABLED                // NC
                      | GPIO_P_MODEH_MODE15_DISABLED;               // NC
    GPIO->P[4].DOUT   = 0;
    GPIO->P[4].OVTDIS = 0;

    // Port F
    GPIO->P[5].CTRL   = GPIO_P_CTRL_DRIVESTRENGTHALT_STRONG | (5 << _GPIO_P_CTRL_SLEWRATEALT_SHIFT)
                      | GPIO_P_CTRL_DRIVESTRENGTH_STRONG | (5 << _GPIO_P_CTRL_SLEWRATE_SHIFT);
    GPIO->P[5].MODEL  = GPIO_P_MODEL_MODE0_PUSHPULL         // DBG_SWCLK - Location 0
                      | GPIO_P_MODEL_MODE1_PUSHPULL         // DBG_SWDIO - Location 0
                      | GPIO_P_MODEL_MODE2_PUSHPULL         // DBG_SWO (UART1_TX - Location 5)
                      | GPIO_P_MODEL_MODE3_DISABLED         // NC
                      | GPIO_P_MODEL_MODE4_DISABLED         // NC
                      | GPIO_P_MODEL_MODE5_DISABLED         // NC
                      | GPIO_P_MODEL_MODE6_DISABLED         // NC
                      | GPIO_P_MODEL_MODE7_DISABLED;        // NC
    GPIO->P[5].MODEH  = GPIO_P_MODEH_MODE8_DISABLED         // NC
                      | GPIO_P_MODEH_MODE9_DISABLED         // NC
                      | GPIO_P_MODEH_MODE10_DISABLED        // NC
                      | GPIO_P_MODEH_MODE11_DISABLED        // NC
                      | GPIO_P_MODEH_MODE12_DISABLED        // NC
                      | GPIO_P_MODEH_MODE13_DISABLED        // NC
                      | GPIO_P_MODEH_MODE14_DISABLED        // NC
                      | GPIO_P_MODEH_MODE15_DISABLED;       // NC
    GPIO->P[5].DOUT   = 0;
    GPIO->P[5].OVTDIS = 0;

    // Debugger Route
    GPIO->ROUTEPEN &= ~(GPIO_ROUTEPEN_TDIPEN | GPIO_ROUTEPEN_TDOPEN); // Disable JTAG

    // External interrupts
    GPIO->EXTIPSELL = GPIO_EXTIPSELL_EXTIPSEL0_PORTA            // NU
                    | GPIO_EXTIPSELL_EXTIPSEL1_PORTA            // NU
                    | GPIO_EXTIPSELL_EXTIPSEL2_PORTA            // NU
                    | GPIO_EXTIPSELL_EXTIPSEL3_PORTA            // NU
                    | GPIO_EXTIPSELL_EXTIPSEL4_PORTA            // NU
                    | GPIO_EXTIPSELL_EXTIPSEL5_PORTA            // NU
                    | GPIO_EXTIPSELL_EXTIPSEL6_PORTA            // NU
                    | GPIO_EXTIPSELL_EXTIPSEL7_PORTA;           // NU
    GPIO->EXTIPSELH = GPIO_EXTIPSELH_EXTIPSEL8_PORTA            // NU
                    | GPIO_EXTIPSELH_EXTIPSEL9_PORTA            // NU
                    | GPIO_EXTIPSELH_EXTIPSEL10_PORTA           // NU
                    | GPIO_EXTIPSELH_EXTIPSEL11_PORTA           // NU
                    | GPIO_EXTIPSELH_EXTIPSEL12_PORTA           // NU
                    | GPIO_EXTIPSELH_EXTIPSEL13_PORTA           // NU
                    | GPIO_EXTIPSELH_EXTIPSEL14_PORTA           // NU
                    | GPIO_EXTIPSELH_EXTIPSEL15_PORTA;          // NU

    GPIO->EXTIPINSELL = GPIO_EXTIPINSELL_EXTIPINSEL0_PIN0       // NU
                      | GPIO_EXTIPINSELL_EXTIPINSEL1_PIN0       // NU
                      | GPIO_EXTIPINSELL_EXTIPINSEL2_PIN0       // NU
                      | GPIO_EXTIPINSELL_EXTIPINSEL3_PIN0       // NU
                      | GPIO_EXTIPINSELL_EXTIPINSEL4_PIN4       // NU
                      | GPIO_EXTIPINSELL_EXTIPINSEL5_PIN4       // NU
                      | GPIO_EXTIPINSELL_EXTIPINSEL6_PIN4       // NU
                      | GPIO_EXTIPINSELL_EXTIPINSEL7_PIN4;      // NU
    GPIO->EXTIPINSELH = GPIO_EXTIPINSELH_EXTIPINSEL8_PIN8       // NU
                      | GPIO_EXTIPINSELH_EXTIPINSEL9_PIN8       // NU
                      | GPIO_EXTIPINSELH_EXTIPINSEL10_PIN8      // NU
                      | GPIO_EXTIPINSELH_EXTIPINSEL11_PIN8      // NU
                      | GPIO_EXTIPINSELH_EXTIPINSEL12_PIN12     // NU
                      | GPIO_EXTIPINSELH_EXTIPINSEL13_PIN12     // NU
                      | GPIO_EXTIPINSELH_EXTIPINSEL14_PIN12     // NU
                      | GPIO_EXTIPINSELH_EXTIPINSEL15_PIN12;    // NU

    GPIO->EXTIRISE = 0; // 
    GPIO->EXTIFALL = 0; // 

    GPIO->IFC = _GPIO_IFC_MASK; // Clear pending IRQs
    IRQ_CLEAR(GPIO_EVEN_IRQn); // Clear pending vector
    IRQ_CLEAR(GPIO_ODD_IRQn); // Clear pending vector
    IRQ_SET_PRIO(GPIO_EVEN_IRQn, 0, 0); // Set priority 0,0 (max)
    IRQ_SET_PRIO(GPIO_ODD_IRQn, 0, 0); // Set priority 0,0 (max)
    IRQ_ENABLE(GPIO_EVEN_IRQn); // Enable vector
    IRQ_ENABLE(GPIO_ODD_IRQn); // Enable vector
    GPIO->IEN = 0; // Enable interrupts
}