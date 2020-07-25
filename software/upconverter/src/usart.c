#include "usart.h"

#if defined(USART0_MODE_SPI)
void usart0_init(uint32_t ulBaud, uint8_t ubMode, uint8_t ubBitMode, int8_t bMISOLocation, int8_t bMOSILocation, uint8_t ubCLKLocation)
{
    if(bMISOLocation > -1 && bMISOLocation > AFCHANLOC_MAX)
        return;

    if(bMOSILocation > -1 && bMOSILocation > AFCHANLOC_MAX)
        return;

    if(ubCLKLocation > AFCHANLOC_MAX)
        return;

    cmu_hfper0_clock_gate(CMU_HFPERCLKEN0_USART0, 1);

    USART0->CMD = USART_CMD_CLEARRX | USART_CMD_CLEARTX | USART_CMD_TXTRIDIS | USART_CMD_RXBLOCKDIS | USART_CMD_TXDIS | USART_CMD_RXDIS;

    USART0->CTRL = USART_CTRL_TXBIL_EMPTY | USART_CTRL_CSMA_NOACTION | (ubBitMode == USART_SPI_MSB_FIRST ? USART_CTRL_MSBF : 0) | (ubMode & 1 ? USART_CTRL_CLKPHA_SAMPLETRAILING : USART_CTRL_CLKPHA_SAMPLELEADING) | (ubMode & 2 ? USART_CTRL_CLKPOL_IDLEHIGH : USART_CTRL_CLKPOL_IDLELOW) | USART_CTRL_SYNC;
    USART0->FRAME = USART_FRAME_DATABITS_EIGHT;
    USART0->CLKDIV = (uint32_t)((((float)HFPER_CLOCK_FREQ / (2.f * ulBaud)) - 1.f) * 256.f);

    USART0->ROUTEPEN = (bMISOLocation >= 0 ? USART_ROUTEPEN_RXPEN : 0) | (bMOSILocation >= 0 ? USART_ROUTEPEN_TXPEN : 0) | USART_ROUTEPEN_CLKPEN;
    USART0->ROUTELOC0 = ((uint32_t)(bMISOLocation >= 0 ? bMISOLocation : 0) << _USART_ROUTELOC0_RXLOC_SHIFT) | ((uint32_t)(bMOSILocation >= 0 ? bMOSILocation : 0) << _USART_ROUTELOC0_TXLOC_SHIFT) | ((uint32_t)ubCLKLocation << _USART_ROUTELOC0_CLKLOC_SHIFT);

    USART0->CMD = USART_CMD_MASTEREN | (bMOSILocation >= 0 ? USART_CMD_TXEN : 0) | (bMISOLocation >= 0 ? USART_CMD_RXEN : 0);
}
uint8_t usart0_spi_transfer_byte(const uint8_t ubData)
{
    while(USART0->STATUS & _USART_STATUS_TXBUFCNT_MASK);

    USART0->CMD = USART_CMD_CLEARRX;

    USART0->TXDATA = ubData;

    while(!(USART0->STATUS & USART_STATUS_TXC));

    return USART0->RXDATA;
}
void usart0_spi_write_byte(const uint8_t ubData, const uint8_t ubWait)
{
    while(!(USART0->STATUS & USART_STATUS_TXBL));

    USART0->TXDATA = ubData;

    while(ubWait && !(USART0->STATUS & USART_STATUS_TXC));
}
#else   // USART0_MODE_SPI
static volatile uint8_t *pubUSART0DMABuffer = NULL;
static volatile uint8_t *pubUSART0FIFO = NULL;
static volatile uint16_t usUSART0FIFOWritePos, usUSART0FIFOReadPos;
static ldma_descriptor_t __attribute__ ((aligned (4))) pUSART0DMADescriptor[2];

void _usart0_rx_isr()
{
    uint32_t ulFlags = USART0->IFC;

    if(ulFlags & USART_IFC_TCMP0)
    {
        ldma_ch_peri_req_disable(USART0_DMA_CHANNEL);

        while(ldma_ch_get_busy(USART0_DMA_CHANNEL));

        uint16_t usDMAXfersLeft = ldma_ch_get_remaining_xfers(USART0_DMA_CHANNEL); // Number of half words left
        uint32_t ulSize = (USART0_DMA_RX_BUFFER_SIZE >> 1) - (usDMAXfersLeft << 1); // Multiply to get number of bytes

        if(ulSize)
        {
            volatile uint8_t *pubDMANextDst = (volatile uint8_t *)ldma_ch_get_next_dst_addr(USART0_DMA_CHANNEL);
            volatile uint8_t *pubDMABufferReadPos = pubDMANextDst >= (pubUSART0DMABuffer + (USART0_DMA_RX_BUFFER_SIZE >> 1)) ? (pubUSART0DMABuffer + (USART0_DMA_RX_BUFFER_SIZE >> 1)) : pubUSART0DMABuffer;

            while(ulSize--)
            {
                pubUSART0FIFO[usUSART0FIFOWritePos++] = *pubDMABufferReadPos++;

                if(usUSART0FIFOWritePos >= USART0_FIFO_SIZE)
                    usUSART0FIFOWritePos = 0;
            }

        }

        while(USART0->STATUS & USART_STATUS_RXDATAV)
        {
            pubUSART0FIFO[usUSART0FIFOWritePos++] = USART0->RXDATA;

            if(usUSART0FIFOWritePos >= USART0_FIFO_SIZE)
                usUSART0FIFOWritePos = 0;
        }

        ldma_ch_load(USART0_DMA_CHANNEL, pUSART0DMADescriptor);
        ldma_ch_peri_req_enable(USART0_DMA_CHANNEL);
    }
}
static void usart0_dma_isr(uint8_t ubError)
{
    if(ubError)
    {
        ldma_ch_load(USART0_DMA_CHANNEL, pUSART0DMADescriptor);

        return;
    }

    volatile uint8_t *pubDMANextDst = (volatile uint8_t *)ldma_ch_get_next_dst_addr(USART0_DMA_CHANNEL);
    volatile uint8_t *pubDMABufferReadPos = pubDMANextDst >= (pubUSART0DMABuffer + (USART0_DMA_RX_BUFFER_SIZE >> 1)) ? pubUSART0DMABuffer : (pubUSART0DMABuffer + (USART0_DMA_RX_BUFFER_SIZE >> 1)); // If next destination address is on the second buffer it means that the first buffer has just gone full, copy from the first, and vice versa

    uint32_t ulSize = (USART0_DMA_RX_BUFFER_SIZE >> 1);

    while(ulSize--)
    {
        pubUSART0FIFO[usUSART0FIFOWritePos++] = *pubDMABufferReadPos++;

        if(usUSART0FIFOWritePos >= USART0_FIFO_SIZE)
            usUSART0FIFOWritePos = 0;
    }
}

void usart0_init(uint32_t ulBaud, uint32_t ulFrameSettings, int8_t bRXLocation, int8_t bTXLocation, int8_t bCTSLocation, int8_t bRTSLocation)
{
    if(bRXLocation > -1 && bRXLocation > AFCHANLOC_MAX)
        return;

    if(bTXLocation > -1 && bTXLocation > AFCHANLOC_MAX)
        return;

    if(bCTSLocation > -1 && bCTSLocation > AFCHANLOC_MAX)
        return;

    if(bRTSLocation > -1 && bRTSLocation > AFCHANLOC_MAX)
        return;

    cmu_hfper0_clock_gate(CMU_HFPERCLKEN0_USART0, 1);

    USART0->CMD = USART_CMD_CLEARRX | USART_CMD_CLEARTX | USART_CMD_TXTRIDIS | USART_CMD_RXBLOCKDIS | USART_CMD_TXDIS | USART_CMD_RXDIS;

    free((uint8_t *)pubUSART0DMABuffer);
    free((uint8_t *)pubUSART0FIFO);

    pubUSART0DMABuffer = (volatile uint8_t *)malloc(USART0_DMA_RX_BUFFER_SIZE);

    if(!pubUSART0DMABuffer)
        return;

    memset((uint8_t *)pubUSART0DMABuffer, 0, USART0_DMA_RX_BUFFER_SIZE);

    pubUSART0FIFO = (volatile uint8_t *)malloc(USART0_FIFO_SIZE);

    if(!pubUSART0FIFO)
    {
        free((void *)pubUSART0DMABuffer);

        return;
    }

    memset((uint8_t *)pubUSART0FIFO, 0, USART0_FIFO_SIZE);

    usUSART0FIFOWritePos = 0;
    usUSART0FIFOReadPos = 0;

    USART0->CTRL = USART_CTRL_TXBIL_HALFFULL | USART_CTRL_CSMA_NOACTION | USART_CTRL_OVS_X16;
    USART0->CTRLX = (bCTSLocation >= 0 ? USART_CTRLX_CTSEN : 0);
    USART0->FRAME = ulFrameSettings;
    USART0->CLKDIV = (uint32_t)((((float)HFPER_CLOCK_FREQ / (16.f * ulBaud)) - 1.f) * 256.f);

    USART0->TIMECMP0 = USART_TIMECMP0_TSTOP_RXACT | USART_TIMECMP0_TSTART_RXEOF | 0x08; // RX Timeout after 8 baud times

    USART0->ROUTEPEN = (bRXLocation >= 0 ? USART_ROUTEPEN_RXPEN : 0) | (bTXLocation >= 0 ? USART_ROUTEPEN_TXPEN : 0) | (bCTSLocation >= 0 ? USART_ROUTEPEN_CTSPEN : 0) | (bRTSLocation >= 0 ? USART_ROUTEPEN_RTSPEN : 0);
    USART0->ROUTELOC0 = ((uint32_t)(bRXLocation >= 0 ? bRXLocation : 0) << _USART_ROUTELOC0_RXLOC_SHIFT) | ((uint32_t)(bTXLocation >= 0 ? bTXLocation : 0) << _USART_ROUTELOC0_TXLOC_SHIFT);
    USART0->ROUTELOC1 = ((uint32_t)(bCTSLocation >= 0 ? bCTSLocation : 0) << _USART_ROUTELOC1_CTSLOC_SHIFT) | ((uint32_t)(bRTSLocation >= 0 ? bRTSLocation : 0) << _USART_ROUTELOC1_RTSLOC_SHIFT);

    USART0->IFC = _USART_IFC_MASK; // Clear all flags
    IRQ_CLEAR(USART0_RX_IRQn); // Clear pending vector
    IRQ_SET_PRIO(USART0_RX_IRQn, 2, 1); // Set priority 2,1
    IRQ_ENABLE(USART0_RX_IRQn); // Enable vector
    USART0->IEN |= USART_IEN_TCMP0; // Enable TCMP0 flag

    ldma_ch_disable(USART0_DMA_CHANNEL);
    ldma_ch_peri_req_disable(USART0_DMA_CHANNEL);
    ldma_ch_req_clear(USART0_DMA_CHANNEL);

    ldma_ch_config(USART0_DMA_CHANNEL, LDMA_CH_REQSEL_SOURCESEL_USART0 | LDMA_CH_REQSEL_SIGSEL_USART0RXDATAV, LDMA_CH_CFG_SRCINCSIGN_DEFAULT, LDMA_CH_CFG_DSTINCSIGN_POSITIVE, LDMA_CH_CFG_ARBSLOTS_DEFAULT, 0);
    ldma_ch_set_isr(USART0_DMA_CHANNEL, usart0_dma_isr);

    pUSART0DMADescriptor[0].CTRL = LDMA_CH_CTRL_DSTMODE_ABSOLUTE | LDMA_CH_CTRL_SRCMODE_ABSOLUTE | LDMA_CH_CTRL_DSTINC_ONE | LDMA_CH_CTRL_SIZE_HALFWORD | LDMA_CH_CTRL_SRCINC_NONE | LDMA_CH_CTRL_IGNORESREQ | LDMA_CH_CTRL_REQMODE_BLOCK | LDMA_CH_CTRL_DONEIFSEN | LDMA_CH_CTRL_BLOCKSIZE_UNIT1 | ((((USART0_DMA_RX_BUFFER_SIZE >> 2) - 1) << _LDMA_CH_CTRL_XFERCNT_SHIFT) & _LDMA_CH_CTRL_XFERCNT_MASK) | LDMA_CH_CTRL_STRUCTTYPE_TRANSFER;
    pUSART0DMADescriptor[0].SRC = (void *)&USART0->RXDOUBLE;
    pUSART0DMADescriptor[0].DST = pubUSART0DMABuffer;
    pUSART0DMADescriptor[0].LINK = 0x00000010 | LDMA_CH_LINK_LINK | LDMA_CH_LINK_LINKMODE_RELATIVE;

    pUSART0DMADescriptor[1].CTRL = pUSART0DMADescriptor[0].CTRL;
    pUSART0DMADescriptor[1].SRC = pUSART0DMADescriptor[0].SRC;
    pUSART0DMADescriptor[1].DST = pubUSART0DMABuffer + (USART0_DMA_RX_BUFFER_SIZE >> 1);
    pUSART0DMADescriptor[1].LINK = 0xFFFFFFF0 | LDMA_CH_LINK_LINK | LDMA_CH_LINK_LINKMODE_RELATIVE;

    ldma_ch_load(USART0_DMA_CHANNEL, pUSART0DMADescriptor);
    ldma_ch_peri_req_enable(USART0_DMA_CHANNEL);
    ldma_ch_enable(USART0_DMA_CHANNEL);

    USART0->CMD = (bTXLocation >= 0 ? USART_CMD_TXEN : 0) | (bRXLocation >= 0 ? USART_CMD_RXEN : 0);
}
void usart0_write_byte(const uint8_t ubData)
{
    while(!(USART0->STATUS & USART_STATUS_TXBL));

    USART0->TXDATA = ubData;
}
uint8_t usart0_read_byte()
{
    if(!usart0_available())
        return 0;

    uint8_t ubData;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        ubData = pubUSART0FIFO[usUSART0FIFOReadPos++];

        if(usUSART0FIFOReadPos >= USART0_FIFO_SIZE)
            usUSART0FIFOReadPos = 0;
    }

    return ubData;
}
uint32_t usart0_available()
{
    return (USART0_FIFO_SIZE + usUSART0FIFOWritePos - usUSART0FIFOReadPos) % USART0_FIFO_SIZE;
}
void usart0_flush()
{
    usUSART0FIFOReadPos = usUSART0FIFOWritePos = 0;
}
#endif  // USART0_MODE_SPI

#if defined(USART1_MODE_SPI)
void usart1_init(uint32_t ulBaud, uint8_t ubMode, uint8_t ubBitMode, int8_t bMISOLocation, int8_t bMOSILocation, uint8_t ubCLKLocation)
{
    if(bMISOLocation > -1 && bMISOLocation > AFCHANLOC_MAX)
        return;

    if(bMOSILocation > -1 && bMOSILocation > AFCHANLOC_MAX)
        return;

    if(ubCLKLocation > AFCHANLOC_MAX)
        return;

    cmu_hfper0_clock_gate(CMU_HFPERCLKEN0_USART1, 1);

    USART1->CMD = USART_CMD_CLEARRX | USART_CMD_CLEARTX | USART_CMD_TXTRIDIS | USART_CMD_RXBLOCKDIS | USART_CMD_TXDIS | USART_CMD_RXDIS;

    USART1->CTRL = USART_CTRL_TXBIL_EMPTY | USART_CTRL_CSMA_NOACTION | (ubBitMode == USART_SPI_MSB_FIRST ? USART_CTRL_MSBF : 0) | (ubMode & 1 ? USART_CTRL_CLKPHA_SAMPLETRAILING : USART_CTRL_CLKPHA_SAMPLELEADING) | (ubMode & 2 ? USART_CTRL_CLKPOL_IDLEHIGH : USART_CTRL_CLKPOL_IDLELOW) | USART_CTRL_SYNC;
    USART1->FRAME = USART_FRAME_DATABITS_EIGHT;
    USART1->CLKDIV = (uint32_t)((((float)HFPER_CLOCK_FREQ / (2.f * ulBaud)) - 1.f) * 256.f);

    USART1->ROUTEPEN = (bMISOLocation >= 0 ? USART_ROUTEPEN_RXPEN : 0) | (bMOSILocation >= 0 ? USART_ROUTEPEN_TXPEN : 0) | USART_ROUTEPEN_CLKPEN;
    USART1->ROUTELOC0 = ((uint32_t)(bMISOLocation >= 0 ? bMISOLocation : 0) << _USART_ROUTELOC0_RXLOC_SHIFT) | ((uint32_t)(bMOSILocation >= 0 ? bMOSILocation : 0) << _USART_ROUTELOC0_TXLOC_SHIFT) | ((uint32_t)ubCLKLocation << _USART_ROUTELOC0_CLKLOC_SHIFT);

    USART1->CMD = USART_CMD_MASTEREN | (bMOSILocation >= 0 ? USART_CMD_TXEN : 0) | (bMISOLocation >= 0 ? USART_CMD_RXEN : 0);
}
uint8_t usart1_spi_transfer_byte(const uint8_t ubData)
{
    while(USART1->STATUS & _USART_STATUS_TXBUFCNT_MASK);

    USART1->CMD = USART_CMD_CLEARRX;

    USART1->TXDATA = ubData;

    while(!(USART1->STATUS & USART_STATUS_TXC));

    return USART1->RXDATA;
}
void usart1_spi_write_byte(const uint8_t ubData, const uint8_t ubWait)
{
    while(!(USART1->STATUS & USART_STATUS_TXBL));

    USART1->TXDATA = ubData;

    while(ubWait && !(USART1->STATUS & USART_STATUS_TXC));
}
#else   // USART1_MODE_SPI
static volatile uint8_t *pubUSART1DMABuffer = NULL;
static volatile uint8_t *pubUSART1FIFO = NULL;
static volatile uint16_t usUSART1FIFOWritePos, usUSART1FIFOReadPos;
static ldma_descriptor_t __attribute__ ((aligned (4))) pUSART1DMADescriptor[2];

void _usart1_rx_isr()
{
    uint32_t ulFlags = USART1->IFC;

    if(ulFlags & USART_IFC_TCMP0)
    {
        ldma_ch_peri_req_disable(USART1_DMA_CHANNEL);

        while(ldma_ch_get_busy(USART1_DMA_CHANNEL));

        uint16_t usDMAXfersLeft = ldma_ch_get_remaining_xfers(USART1_DMA_CHANNEL); // Number of half words left
        uint32_t ulSize = (USART1_DMA_RX_BUFFER_SIZE >> 1) - (usDMAXfersLeft << 1); // Multiply to get number of bytes

        if(ulSize)
        {
            volatile uint8_t *pubDMANextDst = (volatile uint8_t *)ldma_ch_get_next_dst_addr(USART1_DMA_CHANNEL);
            volatile uint8_t *pubDMABufferReadPos = pubDMANextDst >= (pubUSART1DMABuffer + (USART1_DMA_RX_BUFFER_SIZE >> 1)) ? (pubUSART1DMABuffer + (USART1_DMA_RX_BUFFER_SIZE >> 1)) : pubUSART1DMABuffer;

            while(ulSize--)
            {
                pubUSART1FIFO[usUSART1FIFOWritePos++] = *pubDMABufferReadPos++;

                if(usUSART1FIFOWritePos >= USART1_FIFO_SIZE)
                    usUSART1FIFOWritePos = 0;
            }

        }

        while(USART1->STATUS & USART_STATUS_RXDATAV)
        {
            pubUSART1FIFO[usUSART1FIFOWritePos++] = USART1->RXDATA;

            if(usUSART1FIFOWritePos >= USART1_FIFO_SIZE)
                usUSART1FIFOWritePos = 0;
        }

        ldma_ch_load(USART1_DMA_CHANNEL, pUSART1DMADescriptor);
        ldma_ch_peri_req_enable(USART1_DMA_CHANNEL);
    }
}
static void usart1_dma_isr(uint8_t ubError)
{
    if(ubError)
    {
        ldma_ch_load(USART1_DMA_CHANNEL, pUSART1DMADescriptor);

        return;
    }

    volatile uint8_t *pubDMANextDst = (volatile uint8_t *)ldma_ch_get_next_dst_addr(USART1_DMA_CHANNEL);
    volatile uint8_t *pubDMABufferReadPos = pubDMANextDst >= (pubUSART1DMABuffer + (USART1_DMA_RX_BUFFER_SIZE >> 1)) ? pubUSART1DMABuffer : (pubUSART1DMABuffer + (USART1_DMA_RX_BUFFER_SIZE >> 1)); // If next destination address is on the second buffer it means that the first buffer has just gone full, copy from the first, and vice versa

    uint32_t ulSize = (USART1_DMA_RX_BUFFER_SIZE >> 1);

    while(ulSize--)
    {
        pubUSART1FIFO[usUSART1FIFOWritePos++] = *pubDMABufferReadPos++;

        if(usUSART1FIFOWritePos >= USART1_FIFO_SIZE)
            usUSART1FIFOWritePos = 0;
    }
}

void usart1_init(uint32_t ulBaud, uint32_t ulFrameSettings, int8_t bRXLocation, int8_t bTXLocation, int8_t bCTSLocation, int8_t bRTSLocation)
{
    if(bRXLocation > -1 && bRXLocation > AFCHANLOC_MAX)
        return;

    if(bTXLocation > -1 && bTXLocation > AFCHANLOC_MAX)
        return;

    if(bCTSLocation > -1 && bCTSLocation > AFCHANLOC_MAX)
        return;

    if(bRTSLocation > -1 && bRTSLocation > AFCHANLOC_MAX)
        return;

    cmu_hfper0_clock_gate(CMU_HFPERCLKEN0_USART1, 1);

    USART1->CMD = USART_CMD_CLEARRX | USART_CMD_CLEARTX | USART_CMD_TXTRIDIS | USART_CMD_RXBLOCKDIS | USART_CMD_TXDIS | USART_CMD_RXDIS;

    free((uint8_t *)pubUSART1DMABuffer);
    free((uint8_t *)pubUSART1FIFO);

    pubUSART1DMABuffer = (volatile uint8_t *)malloc(USART1_DMA_RX_BUFFER_SIZE);

    if(!pubUSART1DMABuffer)
        return;

    memset((uint8_t *)pubUSART1DMABuffer, 0, USART1_DMA_RX_BUFFER_SIZE);

    pubUSART1FIFO = (volatile uint8_t *)malloc(USART1_FIFO_SIZE);

    if(!pubUSART1FIFO)
    {
        free((void *)pubUSART1DMABuffer);

        return;
    }

    memset((uint8_t *)pubUSART1FIFO, 0, USART1_FIFO_SIZE);

    usUSART1FIFOWritePos = 0;
    usUSART1FIFOReadPos = 0;

    USART1->CTRL = USART_CTRL_TXBIL_HALFFULL | USART_CTRL_CSMA_NOACTION | USART_CTRL_OVS_X16;
    USART1->CTRLX = (bCTSLocation >= 0 ? USART_CTRLX_CTSEN : 0);
    USART1->FRAME = ulFrameSettings;
    USART1->CLKDIV = (uint32_t)((((float)HFPER_CLOCK_FREQ / (16.f * ulBaud)) - 1.f) * 256.f);

    USART1->TIMECMP0 = USART_TIMECMP0_TSTOP_RXACT | USART_TIMECMP0_TSTART_RXEOF | 0x08; // RX Timeout after 8 baud times

    USART1->ROUTEPEN = (bRXLocation >= 0 ? USART_ROUTEPEN_RXPEN : 0) | (bTXLocation >= 0 ? USART_ROUTEPEN_TXPEN : 0) | (bCTSLocation >= 0 ? USART_ROUTEPEN_CTSPEN : 0) | (bRTSLocation >= 0 ? USART_ROUTEPEN_RTSPEN : 0);
    USART1->ROUTELOC0 = ((uint32_t)(bRXLocation >= 0 ? bRXLocation : 0) << _USART_ROUTELOC0_RXLOC_SHIFT) | ((uint32_t)(bTXLocation >= 0 ? bTXLocation : 0) << _USART_ROUTELOC0_TXLOC_SHIFT);
    USART1->ROUTELOC1 = ((uint32_t)(bCTSLocation >= 0 ? bCTSLocation : 0) << _USART_ROUTELOC1_CTSLOC_SHIFT) | ((uint32_t)(bRTSLocation >= 0 ? bRTSLocation : 0) << _USART_ROUTELOC1_RTSLOC_SHIFT);

    USART1->IFC = _USART_IFC_MASK; // Clear all flags
    IRQ_CLEAR(USART1_RX_IRQn); // Clear pending vector
    IRQ_SET_PRIO(USART1_RX_IRQn, 2, 1); // Set priority 2,1
    IRQ_ENABLE(USART1_RX_IRQn); // Enable vector
    USART1->IEN |= USART_IEN_TCMP0; // Enable TCMP0 flag

    ldma_ch_disable(USART1_DMA_CHANNEL);
    ldma_ch_peri_req_disable(USART1_DMA_CHANNEL);
    ldma_ch_req_clear(USART1_DMA_CHANNEL);

    ldma_ch_config(USART1_DMA_CHANNEL, LDMA_CH_REQSEL_SOURCESEL_USART1 | LDMA_CH_REQSEL_SIGSEL_USART1RXDATAV, LDMA_CH_CFG_SRCINCSIGN_DEFAULT, LDMA_CH_CFG_DSTINCSIGN_POSITIVE, LDMA_CH_CFG_ARBSLOTS_DEFAULT, 0);
    ldma_ch_set_isr(USART1_DMA_CHANNEL, usart1_dma_isr);

    pUSART1DMADescriptor[0].CTRL = LDMA_CH_CTRL_DSTMODE_ABSOLUTE | LDMA_CH_CTRL_SRCMODE_ABSOLUTE | LDMA_CH_CTRL_DSTINC_ONE | LDMA_CH_CTRL_SIZE_HALFWORD | LDMA_CH_CTRL_SRCINC_NONE | LDMA_CH_CTRL_IGNORESREQ | LDMA_CH_CTRL_REQMODE_BLOCK | LDMA_CH_CTRL_DONEIFSEN | LDMA_CH_CTRL_BLOCKSIZE_UNIT1 | ((((USART1_DMA_RX_BUFFER_SIZE >> 2) - 1) << _LDMA_CH_CTRL_XFERCNT_SHIFT) & _LDMA_CH_CTRL_XFERCNT_MASK) | LDMA_CH_CTRL_STRUCTTYPE_TRANSFER;
    pUSART1DMADescriptor[0].SRC = (void *)&USART1->RXDOUBLE;
    pUSART1DMADescriptor[0].DST = pubUSART1DMABuffer;
    pUSART1DMADescriptor[0].LINK = 0x00000010 | LDMA_CH_LINK_LINK | LDMA_CH_LINK_LINKMODE_RELATIVE;

    pUSART1DMADescriptor[1].CTRL = pUSART1DMADescriptor[0].CTRL;
    pUSART1DMADescriptor[1].SRC = pUSART1DMADescriptor[0].SRC;
    pUSART1DMADescriptor[1].DST = pubUSART1DMABuffer + (USART1_DMA_RX_BUFFER_SIZE >> 1);
    pUSART1DMADescriptor[1].LINK = 0xFFFFFFF0 | LDMA_CH_LINK_LINK | LDMA_CH_LINK_LINKMODE_RELATIVE;

    ldma_ch_load(USART1_DMA_CHANNEL, pUSART1DMADescriptor);
    ldma_ch_peri_req_enable(USART1_DMA_CHANNEL);
    ldma_ch_enable(USART1_DMA_CHANNEL);

    USART1->CMD = (bTXLocation >= 0 ? USART_CMD_TXEN : 0) | (bRXLocation >= 0 ? USART_CMD_RXEN : 0);
}
void usart1_write_byte(const uint8_t ubData)
{
    while(!(USART1->STATUS & USART_STATUS_TXBL));

    USART1->TXDATA = ubData;
}
uint8_t usart1_read_byte()
{
    if(!usart1_available())
        return 0;

    uint8_t ubData;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        ubData = pubUSART1FIFO[usUSART1FIFOReadPos++];

        if(usUSART1FIFOReadPos >= USART1_FIFO_SIZE)
            usUSART1FIFOReadPos = 0;
    }

    return ubData;
}
uint32_t usart1_available()
{
    return (USART1_FIFO_SIZE + usUSART1FIFOWritePos - usUSART1FIFOReadPos) % USART1_FIFO_SIZE;
}
void usart1_flush()
{
    usUSART1FIFOReadPos = usUSART1FIFOWritePos = 0;
}
#endif  // USART1_MODE_SPI