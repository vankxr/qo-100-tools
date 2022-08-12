#ifndef __OCXO_H__
#define __OCXO_H__

#include <em_device.h>
#include <stdlib.h>
#include "cmu.h"
#include "atomic.h"
#include "gpio.h"
#include "usart.h"
#include "systick.h"

typedef void (* ocxo_tick_count_done_callback_t)(uint16_t, uint64_t);
typedef void (* ocxo_tick_callback_t)();

#define OCXO_COUNT_OK               0
#define OCXO_COUNT_NOT_POWERED      1
#define OCXO_COUNT_ALREADY_RUNNING  2
#define OCXO_COUNT_TICK_NOT_PRESENT 3
#define OCXO_COUNT_FAIL             255

#define OCXO_WARMUP_TIME            300000 // 5 minutes
#define OCXO_FREQUENCY              10000000 // 10 MHz
#define OCXO_CONTROL_VOLTAGE_REF    4096.f
#define OCXO_CONTROL_VOLTAGE_MAX    4000.f
#define OCXO_CONTROL_VOLTAGE_MIN    0.f

uint8_t ocxo_init();
void gps_timepulse_isr();

void ocxo_power_down();
void ocxo_power_up();
uint64_t ocxo_get_warmup_time();

void ocxo_set_control_voltage(float fVCont);
float ocxo_get_control_voltage();
void ocxo_set_control_voltage_word(uint16_t usVCont);
uint16_t ocxo_get_control_voltage_word();

uint8_t ocxo_count_ticks(uint16_t usTicks, ocxo_tick_count_done_callback_t pfCallback);
uint16_t ocxo_count_get_ticks();
uint8_t ocxo_count_is_tick_present();
uint8_t ocxo_count_is_running();

#endif // __OCXO_H__