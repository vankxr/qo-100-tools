#include <em_device.h>

extern void _estack(); // Not really a function, just to be compatible with array later

extern uint32_t _svect; // ISR Vectors
extern uint32_t _evect;

extern uint32_t _stext; // Main program
extern uint32_t _etext;

extern uint32_t _siiram0; // RAM code source
extern uint32_t _siram0; // RAM code destination
extern uint32_t _eiram0;

extern uint32_t _sidata; // Data source
extern uint32_t _sdata; // Data destination
extern uint32_t _edata;

extern uint32_t _sbss; // BSS destination
extern uint32_t _ebss;

extern uint32_t _end;


void _default_isr()
{
    while(1);
}

void __attribute__ ((weak)) __libc_init_array()
{

}

extern int init();
extern int main();

#define DEFAULT_ISR "_default_isr"

void _reset_isr()
{
    volatile uint32_t *src, *dst;

    src = &_siiram0;
    dst = &_siram0;

    while (dst < &_eiram0) // Copy RAM code
        *(dst++) = *(src++);

    src = &_sidata;
    dst = &_sdata;

    while (dst < &_edata) // Copy data
        *(dst++) = *(src++);

    src = 0;
    dst = &_sbss;

    while (dst < &_ebss) // Zero BSS
        *(dst++) = 0;

    __libc_init_array();

    SCB->VTOR = (uint32_t)&_svect; // ISR Vectors offset
    SCB->AIRCR = 0x05FA0000 | (5 << 8); // Interrupt priority - 2 bits Group, 0 bits Sub-group

    init();
    main();

    __disable_irq();
    while(1);
}

void _nmi_isr()                           __attribute__ ((weak,  alias (DEFAULT_ISR)));
void _hardfault_isr()                     __attribute__ ((weak,  alias (DEFAULT_ISR)));
void _svc_isr()                           __attribute__ ((weak,  alias (DEFAULT_ISR)));
void _pendsv_isr()                        __attribute__ ((weak,  alias (DEFAULT_ISR)));
void _systick_isr()                       __attribute__ ((weak,  alias (DEFAULT_ISR)));
void _emu_isr()                           __attribute__ ((weak,  alias (DEFAULT_ISR)));
void _wdog0_isr()                         __attribute__ ((weak,  alias (DEFAULT_ISR)));
void _ldma_isr()                          __attribute__ ((weak,  alias (DEFAULT_ISR)));
void _smu_isr()                           __attribute__ ((weak,  alias (DEFAULT_ISR)));
void _gpio_even_isr()                     __attribute__ ((weak,  alias (DEFAULT_ISR)));
void _timer0_isr()                        __attribute__ ((weak,  alias (DEFAULT_ISR)));
void _usart0_isr()                        __attribute__ ((weak,  alias (DEFAULT_ISR)));
void _acmp0_1_isr()                       __attribute__ ((weak,  alias (DEFAULT_ISR)));
void _adc0_isr()                          __attribute__ ((weak,  alias (DEFAULT_ISR)));
void _i2c0_isr()                          __attribute__ ((weak,  alias (DEFAULT_ISR)));
void _i2c1_isr()                          __attribute__ ((weak,  alias (DEFAULT_ISR)));
void _gpio_odd_isr()                      __attribute__ ((weak,  alias (DEFAULT_ISR)));
void _timer1_isr()                        __attribute__ ((weak,  alias (DEFAULT_ISR)));
void _usart1_isr()                        __attribute__ ((weak,  alias (DEFAULT_ISR)));
void _usart2_isr()                        __attribute__ ((weak,  alias (DEFAULT_ISR)));
void _uart0_isr()                         __attribute__ ((weak,  alias (DEFAULT_ISR)));
void _leuart0_isr()                       __attribute__ ((weak,  alias (DEFAULT_ISR)));
void _letimer0_isr()                      __attribute__ ((weak,  alias (DEFAULT_ISR)));
void _pcnt0_isr()                         __attribute__ ((weak,  alias (DEFAULT_ISR)));
void _rtcc_isr()                          __attribute__ ((weak,  alias (DEFAULT_ISR)));
void _cmu_isr()                           __attribute__ ((weak,  alias (DEFAULT_ISR)));
void _msc_isr()                           __attribute__ ((weak,  alias (DEFAULT_ISR)));
void _crypto0_trng0_isr()                 __attribute__ ((weak,  alias (DEFAULT_ISR)));
void _cryotimer_isr()                     __attribute__ ((weak,  alias (DEFAULT_ISR)));
void _usart3_isr()                        __attribute__ ((weak,  alias (DEFAULT_ISR)));
void _wtimer0_isr()                       __attribute__ ((weak,  alias (DEFAULT_ISR)));
void _wtimer1_isr()                       __attribute__ ((weak,  alias (DEFAULT_ISR)));
void _vdac0_isr()                         __attribute__ ((weak,  alias (DEFAULT_ISR)));
void _csen_isr()                          __attribute__ ((weak,  alias (DEFAULT_ISR)));
void _lesense_isr()                       __attribute__ ((weak,  alias (DEFAULT_ISR)));
void _lcd_isr()                           __attribute__ ((weak,  alias (DEFAULT_ISR)));
void _can0_isr()                          __attribute__ ((weak,  alias (DEFAULT_ISR)));

__attribute__ ((section(".isr_vector"))) void (* const g_pfnVectors[])() = {
    _estack,
    _reset_isr,
    _nmi_isr,
    _hardfault_isr,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    _svc_isr,
    0,
    0,
    _pendsv_isr,
    _systick_isr,
    _emu_isr,
    _wdog0_isr,
    _ldma_isr,
    _smu_isr,
    _gpio_even_isr,
    _timer0_isr,
    _usart0_isr,
    _acmp0_1_isr,
    _adc0_isr,
    _i2c0_isr,
    _i2c1_isr,
    _gpio_odd_isr,
    _timer1_isr,
    _usart1_isr,
    _usart2_isr,
    _uart0_isr,
    _leuart0_isr,
    _letimer0_isr,
    _pcnt0_isr,
    _rtcc_isr,
    _cmu_isr,
    _msc_isr,
    _crypto0_trng0_isr,
    _cryotimer_isr,
    _usart3_isr,
    _wtimer0_isr,
    _wtimer1_isr,
    _vdac0_isr,
    _csen_isr,
    _lesense_isr,
    _lcd_isr,
    _can0_isr
};
