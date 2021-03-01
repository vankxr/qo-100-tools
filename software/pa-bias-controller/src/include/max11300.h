#ifndef __MAX11300_H__
#define __MAX11300_H__

#include <em_device.h>
#include "utils.h"
#include "systick.h"
#include "atomic.h"
#include "gpio.h"
#include "usart.h"

// Registers
#define MAX11300_REG_DEVICE_ID              0x00
#define MAX11300_REG_INTERRUPT              0x01
#define MAX11300_REG_ADC_STATUS_L           0x02
#define MAX11300_REG_ADC_STATUS_H           0x03
#define MAX11300_REG_DAC_OC_STATUS_L        0x04
#define MAX11300_REG_DAC_OC_STATUS_H        0x05
#define MAX11300_REG_GPI_STATUS_L           0x06
#define MAX11300_REG_GPI_STATUS_H           0x07
#define MAX11300_REG_INT_TEMP               0x08
#define MAX11300_REG_EXT1_TEMP              0x09
#define MAX11300_REG_EXT2_TEMP              0x0A
#define MAX11300_REG_GPI_DATA_L             0x0B
#define MAX11300_REG_GPI_DATA_H             0x0C
#define MAX11300_REG_GPO_DATA_L             0x0D
#define MAX11300_REG_GPO_DATA_H             0x0E
#define MAX11300_REG_DEVICE_CTRL            0x10
#define MAX11300_REG_INTERRUPT_MASK         0x11
#define MAX11300_REG_GPI_IRQ_MODE_L         0x12
#define MAX11300_REG_GPI_IRQ_MODE_M         0x13
#define MAX11300_REG_GPI_IRQ_MODE_H         0x14
#define MAX11300_REG_DAC_PRESETn(n)         (0x16 + (n))
#define MAX11300_REG_TEMP_MON_CONFIG        0x18
#define MAX11300_REG_INT_TEMP_THRESH_HIGH   0x19
#define MAX11300_REG_INT_TEMP_THRESH_LOW    0x1A
#define MAX11300_REG_EXT1_TEMP_THRESH_HIGH  0x1B
#define MAX11300_REG_EXT1_TEMP_THRESH_LOW   0x1C
#define MAX11300_REG_EXT2_TEMP_THRESH_HIGH  0x1D
#define MAX11300_REG_EXT2_TEMP_THRESH_LOW   0x1E
#define MAX11300_REG_PORTn_CONFIG(n)        (0x20 + (n))
#define MAX11300_REG_PORTn_ADC_DATA(n)      (0x40 + (n))
#define MAX11300_REG_PORTn_DAC_DATA(n)      (0x60 + (n))

// MAX11300_REG_INTERRUPT
#define MAX11300_REG_INTERRUPT_ADCFLAG  0x0001
#define MAX11300_REG_INTERRUPT_ADCDR    0x0002
#define MAX11300_REG_INTERRUPT_ADCDM    0x0004
#define MAX11300_REG_INTERRUPT_GPIDR    0x0008
#define MAX11300_REG_INTERRUPT_GPIDM    0x0010
#define MAX11300_REG_INTERRUPT_DACOI    0x0020
#define MAX11300_REG_INTERRUPT_TMPINT   0x01C0
#define MAX11300_REG_INTERRUPT_TMPEXT1  0x0E00
#define MAX11300_REG_INTERRUPT_TMPEXT2  0x7000
#define MAX11300_REG_INTERRUPT_VMON     0x8000

// MAX11300_REG_DEVICE_CTRL
#define MAX11300_REG_DEVICE_CTRL_ADCCTL_IDLE            0x0000
#define MAX11300_REG_DEVICE_CTRL_ADCCTL_SINGLE_SWEEP    0x0001
#define MAX11300_REG_DEVICE_CTRL_ADCCTL_SINGLE_CONV     0x0002
#define MAX11300_REG_DEVICE_CTRL_ADCCTL_CONTINUOUS      0x0003
#define MAX11300_REG_DEVICE_CTRL_DACCTL_SEQUENTIAL      0x0000
#define MAX11300_REG_DEVICE_CTRL_DACCTL_IMMEDIATE       0x0004
#define MAX11300_REG_DEVICE_CTRL_DACCTL_PRESET1         0x0008
#define MAX11300_REG_DEVICE_CTRL_DACCTL_PRESET2         0x000C
#define MAX11300_REG_DEVICE_CTRL_ADCCONV_200KSPS        0x0000
#define MAX11300_REG_DEVICE_CTRL_ADCCONV_250KSPS        0x0010
#define MAX11300_REG_DEVICE_CTRL_ADCCONV_333KSPS        0x0020
#define MAX11300_REG_DEVICE_CTRL_ADCCONV_400KSPS        0x0030
#define MAX11300_REG_DEVICE_CTRL_DACREF_EXTERNAL        0x0000
#define MAX11300_REG_DEVICE_CTRL_DACREF_INTERNAL        0x0040
#define MAX11300_REG_DEVICE_CTRL_THSHDN                 0x0080
#define MAX11300_REG_DEVICE_CTRL_TMPCTL_INT             0x0100
#define MAX11300_REG_DEVICE_CTRL_TMPCTL_EXT1            0x0200
#define MAX11300_REG_DEVICE_CTRL_TMPCTL_EXT2            0x0400
#define MAX11300_REG_DEVICE_CTRL_TMPPER_DEFAULT         0x0000
#define MAX11300_REG_DEVICE_CTRL_TMPPER_EXTENDED        0x0800
#define MAX11300_REG_DEVICE_CTRL_RS_CANCEL              0x1000
#define MAX11300_REG_DEVICE_CTRL_LPEN                   0x2000
#define MAX11300_REG_DEVICE_CTRL_BRST_DEFAULT           0x0000
#define MAX11300_REG_DEVICE_CTRL_BRST_CONTEXTUAL        0x4000
#define MAX11300_REG_DEVICE_CTRL_RESET                  0x8000

// MAX11300_REG_TEMP_MON_CONFIG
#define MAX11300_REG_TEMP_MON_CONFIG_TMPINTMONCFG_4_SAMPLES     0x0000
#define MAX11300_REG_TEMP_MON_CONFIG_TMPINTMONCFG_8_SAMPLES     0x0001
#define MAX11300_REG_TEMP_MON_CONFIG_TMPINTMONCFG_16_SAMPLES    0x0002
#define MAX11300_REG_TEMP_MON_CONFIG_TMPINTMONCFG_32_SAMPLES    0x0003
#define MAX11300_REG_TEMP_MON_CONFIG_TMPEXT1MONCFG_4_SAMPLES    0x0000
#define MAX11300_REG_TEMP_MON_CONFIG_TMPEXT1MONCFG_8_SAMPLES    0x0004
#define MAX11300_REG_TEMP_MON_CONFIG_TMPEXT1MONCFG_16_SAMPLES   0x0008
#define MAX11300_REG_TEMP_MON_CONFIG_TMPEXT1MONCFG_32_SAMPLES   0x000C
#define MAX11300_REG_TEMP_MON_CONFIG_TMPEXT2MONCFG_4_SAMPLES    0x0000
#define MAX11300_REG_TEMP_MON_CONFIG_TMPEXT2MONCFG_8_SAMPLES    0x0010
#define MAX11300_REG_TEMP_MON_CONFIG_TMPEXT2MONCFG_16_SAMPLES   0x0020
#define MAX11300_REG_TEMP_MON_CONFIG_TMPEXT2MONCFG_32_SAMPLES   0x0030

// MAX11300_REG_PORTn_CONFIG
#define MAX11300_REG_PORTn_CONFIG_FUNCPRM_ASSOCIATED_PORT(n)            ((n) & 0x0001F)
#define MAX11300_REG_PORTn_CONFIG_FUNCPRM_NSAMPLES_1                    0x0000
#define MAX11300_REG_PORTn_CONFIG_FUNCPRM_NSAMPLES_2                    0x0020
#define MAX11300_REG_PORTn_CONFIG_FUNCPRM_NSAMPLES_4                    0x0040
#define MAX11300_REG_PORTn_CONFIG_FUNCPRM_NSAMPLES_8                    0x0060
#define MAX11300_REG_PORTn_CONFIG_FUNCPRM_NSAMPLES_16                   0x0080
#define MAX11300_REG_PORTn_CONFIG_FUNCPRM_NSAMPLES_32                   0x00A0
#define MAX11300_REG_PORTn_CONFIG_FUNCPRM_NSAMPLES_64                   0x00C0
#define MAX11300_REG_PORTn_CONFIG_FUNCPRM_NSAMPLES_128                  0x00E0
#define MAX11300_REG_PORTn_CONFIG_FUNCPRM_RANGE_NONE                    0x0000
#define MAX11300_REG_PORTn_CONFIG_FUNCPRM_RANGE_0_P10                   0x0100
#define MAX11300_REG_PORTn_CONFIG_FUNCPRM_RANGE_M5_P5                   0x0200
#define MAX11300_REG_PORTn_CONFIG_FUNCPRM_RANGE_M10_0                   0x0300
#define MAX11300_REG_PORTn_CONFIG_FUNCPRM_RANGE_ADC_0_P2p5_DAC_M5_P5    0x0400
#define MAX11300_REG_PORTn_CONFIG_FUNCPRM_RANGE_ADC_0_P2p5_DAC_0_P10    0x0600
#define MAX11300_REG_PORTn_CONFIG_FUNCPRM_AVR_INTERNAL                  0x0000
#define MAX11300_REG_PORTn_CONFIG_FUNCPRM_AVR_EXTERNAL                  0x0800
#define MAX11300_REG_PORTn_CONFIG_FUNCPRM_INV                           0x0800
#define MAX11300_REG_PORTn_CONFIG_FUNCID_HIZ                            0x0000
#define MAX11300_REG_PORTn_CONFIG_FUNCID_DIG_IN_PROG_LEVEL              0x1000
#define MAX11300_REG_PORTn_CONFIG_FUNCID_BIDIR_LEVEL_TRANS              0x2000
#define MAX11300_REG_PORTn_CONFIG_FUNCID_DIG_OUT_PROG_LEVEL             0x3000
#define MAX11300_REG_PORTn_CONFIG_FUNCID_UNIDIR_LEVEL_TRANS             0x4000
#define MAX11300_REG_PORTn_CONFIG_FUNCID_ANA_OUT                        0x5000
#define MAX11300_REG_PORTn_CONFIG_FUNCID_ANA_OUT_MON                    0x6000
#define MAX11300_REG_PORTn_CONFIG_FUNCID_SINGLE_ANA_IN_POS              0x7000
#define MAX11300_REG_PORTn_CONFIG_FUNCID_DIFF_ANA_IN_POS                0x8000
#define MAX11300_REG_PORTn_CONFIG_FUNCID_DIFF_ANA_IN_NEG                0x9000
#define MAX11300_REG_PORTn_CONFIG_FUNCID_ANA_OUT_DIFF_ANA_IN_NEG        0xA000
#define MAX11300_REG_PORTn_CONFIG_FUNCID_GPI_CONTROLLED_SWITCH          0xB000
#define MAX11300_REG_PORTn_CONFIG_FUNCID_REG_CONTROLLED_SWITCH          0xC000

// Constants
#define MAX11300_DAC_INTERNAL_REF 2500.f // mV
#define MAX11300_DAC_EXTERNAL_REF 0.f // mV
#define MAX11300_ADC_INTERNAL_REF 2500.f // mV
#define MAX11300_ADC_EXTERNAL_REF 2500.f // mV

uint8_t max11300_init();
void max11300_isr();

void max11300_reset();

uint16_t max11300_read_device_id();

void max11300_config(uint16_t usConfig);

void max11300_int_temp_config(uint8_t ubEnable, uint16_t usSamples, float fLowThresh, float fHighThresh);
float max11300_int_temp_read();
void max11300_ext1_temp_config(uint8_t ubEnable, uint16_t usSamples, float fLowThresh, float fHighThresh);
float max11300_ext1_temp_read();
void max11300_ext2_temp_config(uint8_t ubEnable, uint16_t usSamples, float fLowThresh, float fHighThresh);
float max11300_ext2_temp_read();

void max11300_dac_set_preset(uint8_t ubIndex, uint16_t usData);
uint16_t max11300_dac_get_preset(uint8_t ubIndex);

void max11300_port_config(uint8_t ubIndex, uint16_t usConfig);
void max11300_port_gpo_set(uint8_t ubIndex, uint8_t ubState);
uint8_t max11300_port_gpo_get(uint8_t ubIndex);
uint8_t max11300_port_gpi_get(uint8_t ubIndex);
void max11300_port_dac_set_data(uint8_t ubIndex, uint16_t usData);
uint16_t max11300_port_dac_get_data(uint8_t ubIndex);
uint16_t max11300_port_adc_get_data(uint8_t ubIndex);

#endif  // __MAX11300_H__
