#include "i2c.h"

#if defined(I2C0_MODE_MASTER)
void i2c0_init(uint8_t ubMode, uint8_t ubSCLLocation, uint8_t ubSDALocation)
{
    if(ubSCLLocation > AFCHANLOC_MAX)
        return;

    if(ubSDALocation > AFCHANLOC_MAX)
        return;

    cmu_hfper0_clock_gate(CMU_HFPERCLKEN0_I2C0, 1);

    I2C0->CTRL = I2C_CTRL_CLHR_STANDARD | I2C_CTRL_TXBIL_EMPTY;
    I2C0->ROUTEPEN = I2C_ROUTEPEN_SCLPEN | I2C_ROUTEPEN_SDAPEN;
    I2C0->ROUTELOC0 = ((uint32_t)ubSCLLocation << _I2C_ROUTELOC0_SCLLOC_SHIFT) | ((uint32_t)ubSDALocation << _I2C_ROUTELOC0_SDALOC_SHIFT);

    if(ubMode == I2C_NORMAL)
        I2C0->CLKDIV = (((HFPERC_CLOCK_FREQ / 100000) - 8) / 8) - 1;
    else if(ubMode == I2C_FAST)
        I2C0->CLKDIV = (((HFPERC_CLOCK_FREQ / 400000) - 8) / 8) - 1;

    I2C0->CTRL |= I2C_CTRL_EN;
    I2C0->CMD = I2C_CMD_ABORT;

    while(I2C0->STATE & I2C_STATE_BUSY);
}
uint8_t i2c0_transmit(uint8_t ubAddress, uint8_t *pubSrc, uint32_t ulCount, uint8_t ubStop)
{
    I2C0->IFC = _I2C_IFC_MASK;

    I2C0->CMD = I2C_CMD_START;

    while(!(I2C0->IF & (I2C_IF_START | I2C_IF_RSTART | I2C_IF_ARBLOST | I2C_IF_BUSERR)));

    if(I2C0->IF & (I2C_IF_ARBLOST | I2C_IF_BUSERR))
    {
        I2C0->CMD = I2C_CMD_ABORT;

        return 0;
    }

    I2C0->TXDATA = ubAddress;

    while(!(I2C0->IF & (I2C_IF_ACK | I2C_IF_NACK | I2C_IF_ARBLOST | I2C_IF_BUSERR)));

    if(I2C0->IF & (I2C_IF_ARBLOST | I2C_IF_BUSERR))
    {
        I2C0->CMD = I2C_CMD_ABORT;

        return 0;
    }
    else if(I2C0->IF & I2C_IF_NACK)
    {
        I2C0->CMD = I2C_CMD_STOP;

        while(I2C0->IFC & (I2C_IFC_MSTOP | I2C_IFC_ARBLOST | I2C_IF_BUSERR));

        if(I2C0->IF & (I2C_IF_ARBLOST | I2C_IF_BUSERR))
            I2C0->CMD = I2C_CMD_ABORT;

        return 0;
    }

    if(ulCount)
        do
        {
            if(ubAddress & 1) // Read
            {
                while(!(I2C0->IF & (I2C_IF_RXDATAV | I2C_IF_ARBLOST | I2C_IF_BUSERR)));

                if(I2C0->IF & (I2C_IF_ARBLOST | I2C_IF_BUSERR))
                {
                    I2C0->CMD = I2C_CMD_ABORT;

                    return 0;
                }

                *pubSrc++ = I2C0->RXDATA;

                if(ulCount > 1)
                    I2C0->CMD = I2C_CMD_ACK;
                else
                    I2C0->CMD = I2C_CMD_NACK;
            }
            else // Write
            {
                I2C0->IFC = I2C_IFC_ACK;

                I2C0->TXDATA = *pubSrc++;

                while(!(I2C0->IF & (I2C_IF_ACK | I2C_IF_NACK | I2C_IF_ARBLOST | I2C_IF_BUSERR)));

                if(I2C0->IF & (I2C_IF_ARBLOST | I2C_IF_BUSERR))
                {
                    I2C0->CMD = I2C_CMD_ABORT;

                    return 0;
                }
                else if(I2C0->IF & I2C_IF_NACK)
                {
                    I2C0->CMD = I2C_CMD_STOP;

                    while(I2C0->IFC & (I2C_IFC_MSTOP | I2C_IFC_ARBLOST | I2C_IF_BUSERR));

                    if(I2C0->IF & (I2C_IF_ARBLOST | I2C_IF_BUSERR))
                        I2C0->CMD = I2C_CMD_ABORT;

                    return 0;
                }
            }
        } while(--ulCount);

    if(ubStop)
    {
        I2C0->CMD = I2C_CMD_STOP;

        while(I2C0->IFC & (I2C_IFC_MSTOP | I2C_IFC_ARBLOST | I2C_IF_BUSERR));

        if(I2C0->IF & (I2C_IF_ARBLOST | I2C_IF_BUSERR))
        {
            I2C0->CMD = I2C_CMD_ABORT;

            return 0;
        }
    }

    return 1;
}
#else   // I2C0_MODE_MASTER
static i2c_slave_addr_isr_t pfI2C0SlaveAddrISR = NULL;
static i2c_slave_tx_data_isr_t pfI2C0SlaveTXDataISR = NULL;
static i2c_slave_rx_data_isr_t pfI2C0SlaveRXDataISR = NULL;

void _i2c0_isr()
{
    uint32_t ulFlags = I2C0->IFC;

    if(ulFlags & I2C_IFC_BUSHOLD)
    {
        switch(I2C0->STATE & _I2C_STATE_STATE_MASK)
        {
            case I2C_STATE_STATE_ADDR:
            {
                I2C0->CMD = I2C_CMD_CLEARTX;

                volatile uint8_t ubAddress = I2C0->RXDATA;

                if(pfI2C0SlaveAddrISR && pfI2C0SlaveAddrISR(ubAddress))
                    I2C0->CMD = I2C_CMD_ACK;
                else
                    I2C0->CMD = I2C_CMD_NACK;
            }
            break;
            case I2C_STATE_STATE_ADDRACK:
            {
                I2C0->TXDATA = pfI2C0SlaveTXDataISR ? pfI2C0SlaveTXDataISR() : 0xFF;
            }
            break;
            case I2C_STATE_STATE_DATA:
            {
                volatile uint8_t ubData = I2C0->RXDATA;

                if(pfI2C0SlaveRXDataISR && pfI2C0SlaveRXDataISR(ubData))
                    I2C0->CMD = I2C_CMD_ACK;
                else
                    I2C0->CMD = I2C_CMD_NACK;
            }
            break;
            case I2C_STATE_STATE_DATAACK:
            {
                if(!(I2C0->STATE & I2C_STATE_NACKED))
                    I2C0->TXDATA = pfI2C0SlaveTXDataISR ? pfI2C0SlaveTXDataISR() : 0xFF;
            }
            break;
            default:
            {
                I2C0->CMD = I2C_CMD_ABORT;
            }
            break;
        }
    }
}

void i2c0_init(uint8_t ubAddress, uint8_t ubSCLLocation, uint8_t ubSDALocation)
{
    if(ubSCLLocation > AFCHANLOC_MAX)
        return;

    if(ubSDALocation > AFCHANLOC_MAX)
        return;

    cmu_hfper0_clock_gate(CMU_HFPERCLKEN0_I2C0, 1);

    I2C0->CTRL = I2C_CTRL_TXBIL_EMPTY | I2C_CTRL_SLAVE;
    I2C0->CLKDIV = 1;
    I2C0->SADDR = (ubAddress << _I2C_SADDR_ADDR_SHIFT);
    I2C0->SADDRMASK = (0x7F << _I2C_SADDRMASK_MASK_SHIFT);
    I2C0->ROUTEPEN = I2C_ROUTEPEN_SCLPEN | I2C_ROUTEPEN_SDAPEN;
    I2C0->ROUTELOC0 = ((uint32_t)ubSCLLocation << _I2C_ROUTELOC0_SCLLOC_SHIFT) | ((uint32_t)ubSDALocation << _I2C_ROUTELOC0_SDALOC_SHIFT);

    I2C0->IFC = _I2C_IFC_MASK; // Clear all flags
    IRQ_CLEAR(I2C0_IRQn); // Clear pending vector
    IRQ_SET_PRIO(I2C0_IRQn, 2, 0); // Set priority 2,0
    IRQ_ENABLE(I2C0_IRQn); // Enable vector
    I2C0->IEN |= I2C_IEN_BUSHOLD; // Enable BUSHOLD flag

    I2C0->CTRL |= I2C_CTRL_EN;
    I2C0->CMD = I2C_CMD_ABORT;

    while(I2C0->STATE & I2C_STATE_BUSY);
}
void i2c0_set_slave_addr_isr(i2c_slave_addr_isr_t pfISR)
{
    pfI2C0SlaveAddrISR = pfISR;
}
void i2c0_set_slave_tx_data_isr(i2c_slave_tx_data_isr_t pfISR)
{
    pfI2C0SlaveTXDataISR = pfISR;
}
void i2c0_set_slave_rx_data_isr(i2c_slave_rx_data_isr_t pfISR)
{
    pfI2C0SlaveRXDataISR = pfISR;
}
#endif  // I2C0_MODE_MASTER

#if defined(I2C1_MODE_MASTER)
void i2c1_init(uint8_t ubMode, uint8_t ubSCLLocation, uint8_t ubSDALocation)
{
    if(ubSCLLocation > AFCHANLOC_MAX)
        return;

    if(ubSDALocation > AFCHANLOC_MAX)
        return;

    cmu_hfper0_clock_gate(CMU_HFPERCLKEN0_I2C1, 1);

    I2C1->CTRL = I2C_CTRL_CLHR_STANDARD | I2C_CTRL_TXBIL_EMPTY;
    I2C1->ROUTEPEN = I2C_ROUTEPEN_SCLPEN | I2C_ROUTEPEN_SDAPEN;
    I2C1->ROUTELOC0 = ((uint32_t)ubSCLLocation << _I2C_ROUTELOC0_SCLLOC_SHIFT) | ((uint32_t)ubSDALocation << _I2C_ROUTELOC0_SDALOC_SHIFT);

    if(ubMode == I2C_NORMAL)
        I2C1->CLKDIV = (((HFPERC_CLOCK_FREQ / 100000) - 8) / 8) - 1;
    else if(ubMode == I2C_FAST)
        I2C1->CLKDIV = (((HFPERC_CLOCK_FREQ / 400000) - 8) / 8) - 1;

    I2C1->CTRL |= I2C_CTRL_EN;
    I2C1->CMD = I2C_CMD_ABORT;

    while(I2C1->STATE & I2C_STATE_BUSY);
}
uint8_t i2c1_transmit(uint8_t ubAddress, uint8_t *pubSrc, uint32_t ulCount, uint8_t ubStop)
{
    I2C1->IFC = _I2C_IFC_MASK;

    I2C1->CMD = I2C_CMD_START;

    while(!(I2C1->IF & (I2C_IF_START | I2C_IF_RSTART | I2C_IF_ARBLOST | I2C_IF_BUSERR)));

    if(I2C1->IF & (I2C_IF_ARBLOST | I2C_IF_BUSERR))
    {
        I2C1->CMD = I2C_CMD_ABORT;

        return 0;
    }

    I2C1->TXDATA = ubAddress;

    while(!(I2C1->IF & (I2C_IF_ACK | I2C_IF_NACK | I2C_IF_ARBLOST | I2C_IF_BUSERR)));

    if(I2C1->IF & (I2C_IF_ARBLOST | I2C_IF_BUSERR))
    {
        I2C1->CMD = I2C_CMD_ABORT;

        return 0;
    }
    else if(I2C1->IF & I2C_IF_NACK)
    {
        I2C1->CMD = I2C_CMD_STOP;

        while(I2C1->IFC & (I2C_IFC_MSTOP | I2C_IFC_ARBLOST | I2C_IF_BUSERR));

        if(I2C1->IF & (I2C_IF_ARBLOST | I2C_IF_BUSERR))
            I2C1->CMD = I2C_CMD_ABORT;

        return 0;
    }

    if(ulCount)
        do
        {
            if(ubAddress & 1) // Read
            {
                while(!(I2C1->IF & (I2C_IF_RXDATAV | I2C_IF_ARBLOST | I2C_IF_BUSERR)));

                if(I2C1->IF & (I2C_IF_ARBLOST | I2C_IF_BUSERR))
                {
                    I2C1->CMD = I2C_CMD_ABORT;

                    return 0;
                }

                *pubSrc++ = I2C1->RXDATA;

                if(ulCount > 1)
                    I2C1->CMD = I2C_CMD_ACK;
                else
                    I2C1->CMD = I2C_CMD_NACK;
            }
            else // Write
            {
                I2C1->IFC = I2C_IFC_ACK;

                I2C1->TXDATA = *pubSrc++;

                while(!(I2C1->IF & (I2C_IF_ACK | I2C_IF_NACK | I2C_IF_ARBLOST | I2C_IF_BUSERR)));

                if(I2C1->IF & (I2C_IF_ARBLOST | I2C_IF_BUSERR))
                {
                    I2C1->CMD = I2C_CMD_ABORT;

                    return 0;
                }
                else if(I2C1->IF & I2C_IF_NACK)
                {
                    I2C1->CMD = I2C_CMD_STOP;

                    while(I2C1->IFC & (I2C_IFC_MSTOP | I2C_IFC_ARBLOST | I2C_IF_BUSERR));

                    if(I2C1->IF & (I2C_IF_ARBLOST | I2C_IF_BUSERR))
                        I2C1->CMD = I2C_CMD_ABORT;

                    return 0;
                }
            }
        } while(--ulCount);

    if(ubStop)
    {
        I2C1->CMD = I2C_CMD_STOP;

        while(I2C1->IFC & (I2C_IFC_MSTOP | I2C_IFC_ARBLOST | I2C_IF_BUSERR));

        if(I2C1->IF & (I2C_IF_ARBLOST | I2C_IF_BUSERR))
        {
            I2C1->CMD = I2C_CMD_ABORT;

            return 0;
        }
    }

    return 1;
}
#else   // I2C1_MODE_MASTER
static i2c_slave_addr_isr_t pfI2C1SlaveAddrISR = NULL;
static i2c_slave_tx_data_isr_t pfI2C1SlaveTXDataISR = NULL;
static i2c_slave_rx_data_isr_t pfI2C1SlaveRXDataISR = NULL;

void _i2c1_isr()
{
    uint32_t ulFlags = I2C1->IFC;

    if(ulFlags & I2C_IFC_BUSHOLD)
    {
        switch(I2C1->STATE & _I2C_STATE_STATE_MASK)
        {
            case I2C_STATE_STATE_ADDR:
            {
                I2C1->CMD = I2C_CMD_CLEARTX;

                volatile uint8_t ubAddress = I2C1->RXDATA;

                if(pfI2C1SlaveAddrISR && pfI2C1SlaveAddrISR(ubAddress))
                    I2C1->CMD = I2C_CMD_ACK;
                else
                    I2C1->CMD = I2C_CMD_NACK;
            }
            break;
            case I2C_STATE_STATE_ADDRACK:
            {
                I2C1->TXDATA = pfI2C1SlaveTXDataISR ? pfI2C1SlaveTXDataISR() : 0xFF;
            }
            break;
            case I2C_STATE_STATE_DATA:
            {
                volatile uint8_t ubData = I2C1->RXDATA;

                if(pfI2C1SlaveRXDataISR && pfI2C1SlaveRXDataISR(ubData))
                    I2C1->CMD = I2C_CMD_ACK;
                else
                    I2C1->CMD = I2C_CMD_NACK;
            }
            break;
            case I2C_STATE_STATE_DATAACK:
            {
                if(!(I2C1->STATE & I2C_STATE_NACKED))
                    I2C1->TXDATA = pfI2C1SlaveTXDataISR ? pfI2C1SlaveTXDataISR() : 0xFF;
            }
            break;
            default:
            {
                I2C1->CMD = I2C_CMD_ABORT;
            }
            break;
        }
    }
}

void i2c1_init(uint8_t ubAddress, uint8_t ubSCLLocation, uint8_t ubSDALocation)
{
    if(ubSCLLocation > AFCHANLOC_MAX)
        return;

    if(ubSDALocation > AFCHANLOC_MAX)
        return;

    cmu_hfper0_clock_gate(CMU_HFPERCLKEN0_I2C1, 1);

    I2C1->CTRL = I2C_CTRL_TXBIL_EMPTY | I2C_CTRL_SLAVE;
    I2C1->CLKDIV = 1;
    I2C1->SADDR = (ubAddress << _I2C_SADDR_ADDR_SHIFT);
    I2C1->SADDRMASK = (0x7F << _I2C_SADDRMASK_MASK_SHIFT);
    I2C1->ROUTEPEN = I2C_ROUTEPEN_SCLPEN | I2C_ROUTEPEN_SDAPEN;
    I2C1->ROUTELOC0 = ((uint32_t)ubSCLLocation << _I2C_ROUTELOC0_SCLLOC_SHIFT) | ((uint32_t)ubSDALocation << _I2C_ROUTELOC0_SDALOC_SHIFT);

    I2C1->IFC = _I2C_IFC_MASK; // Clear all flags
    IRQ_CLEAR(I2C1_IRQn); // Clear pending vector
    IRQ_SET_PRIO(I2C1_IRQn, 2, 0); // Set priority 2,0
    IRQ_ENABLE(I2C1_IRQn); // Enable vector
    I2C1->IEN |= I2C_IEN_BUSHOLD; // Enable BUSHOLD flag

    I2C1->CTRL |= I2C_CTRL_EN;
    I2C1->CMD = I2C_CMD_ABORT;

    while(I2C1->STATE & I2C_STATE_BUSY);
}
void i2c1_set_slave_addr_isr(i2c_slave_addr_isr_t pfISR)
{
    pfI2C1SlaveAddrISR = pfISR;
}
void i2c1_set_slave_tx_data_isr(i2c_slave_tx_data_isr_t pfISR)
{
    pfI2C1SlaveTXDataISR = pfISR;
}
void i2c1_set_slave_rx_data_isr(i2c_slave_rx_data_isr_t pfISR)
{
    pfI2C1SlaveRXDataISR = pfISR;
}
#endif  // I2C1_MODE_MASTER