#ifndef __I2C_H__
#define __I2C_H__

#include <em_device.h>
#include "cmu.h"
#include "nvic.h"
#include "utils.h"

#define I2C_NORMAL 0
#define I2C_FAST 1

#define I2C_RESTART 0
#define I2C_STOP 1

typedef uint8_t (* i2c_slave_addr_isr_t)(uint8_t);
typedef uint8_t (* i2c_slave_tx_data_isr_t)();
typedef uint8_t (* i2c_slave_rx_data_isr_t)(uint8_t);


//#define I2C0_MODE_MASTER

#if defined(I2C0_MODE_MASTER)
void i2c0_init(uint8_t ubMode, uint8_t ubSCLLocation, uint8_t ubSDALocation);
uint8_t i2c0_transmit(uint8_t ubAddress, uint8_t *pubSrc, uint32_t ulCount, uint8_t ubStop);
static inline uint8_t i2c0_write(uint8_t ubAddress, uint8_t *pubSrc, uint32_t ulCount, uint8_t ubStop)
{
    return i2c0_transmit((ubAddress << 1) & ~0x01, pubSrc, ulCount, ubStop);
}
static inline uint8_t i2c0_read(uint8_t ubAddress, uint8_t *pubDst, uint32_t ulCount, uint8_t ubStop)
{
    return i2c0_transmit((ubAddress << 1) | 0x01, pubDst, ulCount, ubStop);
}
static inline uint8_t i2c0_write_byte(uint8_t ubAddress, uint8_t ubData, uint8_t ubStop)
{
    return i2c0_transmit((ubAddress << 1) & ~0x01, &ubData, 1, ubStop);
}
static inline uint8_t i2c0_read_byte(uint8_t ubAddress, uint8_t ubStop)
{
    uint8_t ubData;

    i2c0_transmit((ubAddress << 1) | 0x01, &ubData, 1, ubStop);

    return ubData;
}
#else   // I2C0_MODE_MASTER
void i2c0_init(uint8_t ubAddress, uint8_t ubSCLLocation, uint8_t ubSDALocation);
void i2c0_set_slave_addr_isr(i2c_slave_addr_isr_t pfISR);
void i2c0_set_slave_tx_data_isr(i2c_slave_tx_data_isr_t pfISR);
void i2c0_set_slave_rx_data_isr(i2c_slave_rx_data_isr_t pfISR);
#endif  // I2C0_MODE_MASTER

#define I2C1_MODE_MASTER

#if defined(I2C1_MODE_MASTER)
void i2c1_init(uint8_t ubMode, uint8_t ubSCLLocation, uint8_t ubSDALocation);
uint8_t i2c1_transmit(uint8_t ubAddress, uint8_t *pubSrc, uint32_t ulCount, uint8_t ubStop);
static inline uint8_t i2c1_write(uint8_t ubAddress, uint8_t *pubSrc, uint32_t ulCount, uint8_t ubStop)
{
    return i2c1_transmit((ubAddress << 1) & ~0x01, pubSrc, ulCount, ubStop);
}
static inline uint8_t i2c1_read(uint8_t ubAddress, uint8_t *pubDst, uint32_t ulCount, uint8_t ubStop)
{
    return i2c1_transmit((ubAddress << 1) | 0x01, pubDst, ulCount, ubStop);
}
static inline uint8_t i2c1_write_byte(uint8_t ubAddress, uint8_t ubData, uint8_t ubStop)
{
    return i2c1_transmit((ubAddress << 1) & ~0x01, &ubData, 1, ubStop);
}
static inline uint8_t i2c1_read_byte(uint8_t ubAddress, uint8_t ubStop)
{
    uint8_t ubData;

    i2c1_transmit((ubAddress << 1) | 0x01, &ubData, 1, ubStop);

    return ubData;
}
#else   // I2C1_MODE_MASTER
void i2c1_init(uint8_t ubAddress, uint8_t ubSCLLocation, uint8_t ubSDALocation);
void i2c1_set_slave_addr_isr(i2c_slave_addr_isr_t pfISR);
void i2c1_set_slave_tx_data_isr(i2c_slave_tx_data_isr_t pfISR);
void i2c1_set_slave_rx_data_isr(i2c_slave_rx_data_isr_t pfISR);
#endif  // I2C1_MODE_MASTER

#endif  // __I2C_H__
