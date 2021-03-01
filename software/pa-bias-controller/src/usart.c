#include "usart.h"

void _putchar(char ch)
{
    usart1_write_byte((uint8_t)ch);
}

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

    USART0->CTRL = USART_CTRL_TXBIL_HALFFULL | USART_CTRL_CSMA_NOACTION | (ubBitMode == USART_SPI_MSB_FIRST ? USART_CTRL_MSBF : 0) | (ubMode & 1 ? USART_CTRL_CLKPHA_SAMPLETRAILING : USART_CTRL_CLKPHA_SAMPLELEADING) | (ubMode & 2 ? USART_CTRL_CLKPOL_IDLEHIGH : USART_CTRL_CLKPOL_IDLELOW) | USART_CTRL_SYNC;
    USART0->FRAME = USART_FRAME_DATABITS_EIGHT;
    USART0->CLKDIV = (uint32_t)((((float)HFPER_CLOCK_FREQ / (2.f * ulBaud)) - 1.f) * 256.f);

    USART0->ROUTEPEN = (bMISOLocation >= 0 ? USART_ROUTEPEN_RXPEN : 0) | (bMOSILocation >= 0 ? USART_ROUTEPEN_TXPEN : 0) | USART_ROUTEPEN_CLKPEN;
    USART0->ROUTELOC0 = ((uint32_t)(bMISOLocation >= 0 ? bMISOLocation : 0) << _USART_ROUTELOC0_RXLOC_SHIFT) | ((uint32_t)(bMOSILocation >= 0 ? bMOSILocation : 0) << _USART_ROUTELOC0_TXLOC_SHIFT) | ((uint32_t)ubCLKLocation << _USART_ROUTELOC0_CLKLOC_SHIFT);

    #if defined(USART0_MODE_SPI_3W)
        USART0->CTRL |= USART_CTRL_LOOPBK;
        USART0->CMD = USART_CMD_MASTEREN | (bMOSILocation >= 0 ? (USART_CMD_TXEN | USART_CMD_RXEN) : 0);
    #else
        USART0->CMD = USART_CMD_MASTEREN | (bMOSILocation >= 0 ? USART_CMD_TXEN : 0) | (bMISOLocation >= 0 ? USART_CMD_RXEN : 0);
    #endif  // USART0_MODE_SPI_3W
}
uint8_t usart0_spi_transfer_byte(const uint8_t ubData)
{
    while(!(USART0->STATUS & USART_STATUS_TXIDLE));

    #if defined(USART0_MODE_SPI_3W)
        USART0->CMD = UART_CMD_TXTRIEN;

        while(!(USART0->STATUS & USART_STATUS_TXTRI));
    #endif  // USART0_MODE_SPI_3W

    USART0->CMD = USART_CMD_CLEARRX;

    USART0->TXDATA = ubData;

    while(!(USART0->STATUS & USART_STATUS_TXC));

    return USART0->RXDATA;
}
void usart0_spi_write_byte(const uint8_t ubData, const uint8_t ubWait)
{
    while(!(USART0->STATUS & USART_STATUS_TXBL));

    #if defined(USART0_MODE_SPI_3W)
        USART0->CMD = USART_CMD_TXTRIDIS;

        while(USART0->STATUS & USART_STATUS_TXTRI);
    #endif  // USART0_MODE_SPI_3W

    USART0->TXDATA = ubData;

    while(ubWait && !(USART0->STATUS & USART_STATUS_TXC));
}
#else   // USART0_MODE_SPI
static volatile uint8_t *pubUSART0DMABuffer = NULL;
static volatile uint8_t *pubUSART0FIFO = NULL;
static volatile uint16_t usUSART0FIFOWritePos, usUSART0FIFOReadPos;
static ldma_descriptor_t __attribute__ ((aligned (4))) pUSART0DMADescriptor[2];

void _usart0_isr()
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
    IRQ_CLEAR(USART0_IRQn); // Clear pending vector
    IRQ_SET_PRIO(USART0_IRQn, 2, 0); // Set priority 2,0
    IRQ_ENABLE(USART0_IRQn); // Enable vector
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

    USART1->CTRL = USART_CTRL_TXBIL_HALFFULL | USART_CTRL_CSMA_NOACTION | (ubBitMode == USART_SPI_MSB_FIRST ? USART_CTRL_MSBF : 0) | (ubMode & 1 ? USART_CTRL_CLKPHA_SAMPLETRAILING : USART_CTRL_CLKPHA_SAMPLELEADING) | (ubMode & 2 ? USART_CTRL_CLKPOL_IDLEHIGH : USART_CTRL_CLKPOL_IDLELOW) | USART_CTRL_SYNC;
    USART1->FRAME = USART_FRAME_DATABITS_EIGHT;
    USART1->CLKDIV = (uint32_t)((((float)HFPER_CLOCK_FREQ / (2.f * ulBaud)) - 1.f) * 256.f);

    USART1->ROUTEPEN = (bMISOLocation >= 0 ? USART_ROUTEPEN_RXPEN : 0) | (bMOSILocation >= 0 ? USART_ROUTEPEN_TXPEN : 0) | USART_ROUTEPEN_CLKPEN;
    USART1->ROUTELOC0 = ((uint32_t)(bMISOLocation >= 0 ? bMISOLocation : 0) << _USART_ROUTELOC0_RXLOC_SHIFT) | ((uint32_t)(bMOSILocation >= 0 ? bMOSILocation : 0) << _USART_ROUTELOC0_TXLOC_SHIFT) | ((uint32_t)ubCLKLocation << _USART_ROUTELOC0_CLKLOC_SHIFT);

    #if defined(USART1_MODE_SPI_3W)
        USART1->CTRL |= USART_CTRL_LOOPBK;
        USART1->CMD = USART_CMD_MASTEREN | (bMOSILocation >= 0 ? (USART_CMD_TXEN | USART_CMD_RXEN) : 0);
    #else
        USART1->CMD = USART_CMD_MASTEREN | (bMOSILocation >= 0 ? USART_CMD_TXEN : 0) | (bMISOLocation >= 0 ? USART_CMD_RXEN : 0);
    #endif  // USART1_MODE_SPI_3W
}
uint8_t usart1_spi_transfer_byte(const uint8_t ubData)
{
    while(!(USART1->STATUS & USART_STATUS_TXIDLE));

    #if defined(USART1_MODE_SPI_3W)
        USART1->CMD = UART_CMD_TXTRIEN;

        while(!(USART1->STATUS & USART_STATUS_TXTRI));
    #endif  // USART1_MODE_SPI_3W

    USART1->CMD = USART_CMD_CLEARRX;

    USART1->TXDATA = ubData;

    while(!(USART1->STATUS & USART_STATUS_TXC));

    return USART1->RXDATA;
}
void usart1_spi_write_byte(const uint8_t ubData, const uint8_t ubWait)
{
    while(!(USART1->STATUS & USART_STATUS_TXBL));

    #if defined(USART1_MODE_SPI_3W)
        USART1->CMD = USART_CMD_TXTRIDIS;

        while(USART1->STATUS & USART_STATUS_TXTRI);
    #endif  // USART1_MODE_SPI_3W

    USART1->TXDATA = ubData;

    while(ubWait && !(USART1->STATUS & USART_STATUS_TXC));
}
#else   // USART1_MODE_SPI
static volatile uint8_t *pubUSART1DMABuffer = NULL;
static volatile uint8_t *pubUSART1FIFO = NULL;
static volatile uint16_t usUSART1FIFOWritePos, usUSART1FIFOReadPos;
static ldma_descriptor_t __attribute__ ((aligned (4))) pUSART1DMADescriptor[2];

void _usart1_isr()
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
    IRQ_CLEAR(USART1_IRQn); // Clear pending vector
    IRQ_SET_PRIO(USART1_IRQn, 2, 0); // Set priority 2,0
    IRQ_ENABLE(USART1_IRQn); // Enable vector
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

#if defined(USART2_MODE_SPI)
void usart2_init(uint32_t ulBaud, uint8_t ubMode, uint8_t ubBitMode, int8_t bMISOLocation, int8_t bMOSILocation, uint8_t ubCLKLocation)
{
    if(bMISOLocation > -1 && bMISOLocation > AFCHANLOC_MAX)
        return;

    if(bMOSILocation > -1 && bMOSILocation > AFCHANLOC_MAX)
        return;

    if(ubCLKLocation > AFCHANLOC_MAX)
        return;

    cmu_hfper0_clock_gate(CMU_HFPERCLKEN0_USART2, 1);

    USART2->CMD = USART_CMD_CLEARRX | USART_CMD_CLEARTX | USART_CMD_TXTRIDIS | USART_CMD_RXBLOCKDIS | USART_CMD_TXDIS | USART_CMD_RXDIS;

    USART2->CTRL = USART_CTRL_TXBIL_HALFFULL | USART_CTRL_CSMA_NOACTION | (ubBitMode == USART_SPI_MSB_FIRST ? USART_CTRL_MSBF : 0) | (ubMode & 1 ? USART_CTRL_CLKPHA_SAMPLETRAILING : USART_CTRL_CLKPHA_SAMPLELEADING) | (ubMode & 2 ? USART_CTRL_CLKPOL_IDLEHIGH : USART_CTRL_CLKPOL_IDLELOW) | USART_CTRL_SYNC;
    USART2->FRAME = USART_FRAME_DATABITS_EIGHT;
    USART2->CLKDIV = (uint32_t)((((float)HFPERB_CLOCK_FREQ / (2.f * ulBaud)) - 1.f) * 256.f);

    USART2->ROUTEPEN = (bMISOLocation >= 0 ? USART_ROUTEPEN_RXPEN : 0) | (bMOSILocation >= 0 ? USART_ROUTEPEN_TXPEN : 0) | USART_ROUTEPEN_CLKPEN;
    USART2->ROUTELOC0 = ((uint32_t)(bMISOLocation >= 0 ? bMISOLocation : 0) << _USART_ROUTELOC0_RXLOC_SHIFT) | ((uint32_t)(bMOSILocation >= 0 ? bMOSILocation : 0) << _USART_ROUTELOC0_TXLOC_SHIFT) | ((uint32_t)ubCLKLocation << _USART_ROUTELOC0_CLKLOC_SHIFT);

    #if defined(USART2_MODE_SPI_3W)
        USART2->CTRL |= USART_CTRL_LOOPBK;
        USART2->CMD = USART_CMD_MASTEREN | (bMOSILocation >= 0 ? (USART_CMD_TXEN | USART_CMD_RXEN) : 0);
    #else
        USART2->CMD = USART_CMD_MASTEREN | (bMOSILocation >= 0 ? USART_CMD_TXEN : 0) | (bMISOLocation >= 0 ? USART_CMD_RXEN : 0);
    #endif  // USART2_MODE_SPI_3W
}
uint8_t usart2_spi_transfer_byte(const uint8_t ubData)
{
    while(!(USART2->STATUS & USART_STATUS_TXIDLE));

    #if defined(USART2_MODE_SPI_3W)
        USART2->CMD = UART_CMD_TXTRIEN;

        while(!(USART2->STATUS & USART_STATUS_TXTRI));
    #endif  // USART2_MODE_SPI_3W

    USART2->CMD = USART_CMD_CLEARRX;

    USART2->TXDATA = ubData;

    while(!(USART2->STATUS & USART_STATUS_TXC));

    return USART2->RXDATA;
}
void usart2_spi_write_byte(const uint8_t ubData, const uint8_t ubWait)
{
    while(!(USART2->STATUS & USART_STATUS_TXBL));

    #if defined(USART2_MODE_SPI_3W)
        USART2->CMD = USART_CMD_TXTRIDIS;

        while(USART2->STATUS & USART_STATUS_TXTRI);
    #endif  // USART2_MODE_SPI_3W

    USART2->TXDATA = ubData;

    while(ubWait && !(USART2->STATUS & USART_STATUS_TXC));
}
#else   // USART2_MODE_SPI
static volatile uint8_t *pubUSART2DMABuffer = NULL;
static volatile uint8_t *pubUSART2FIFO = NULL;
static volatile uint16_t usUSART2FIFOWritePos, usUSART2FIFOReadPos;
static ldma_descriptor_t __attribute__ ((aligned (4))) pUSART2DMADescriptor[2];

void _usart2_isr()
{
    uint32_t ulFlags = USART2->IFC;

    if(ulFlags & USART_IFC_TCMP0)
    {
        ldma_ch_peri_req_disable(USART2_DMA_CHANNEL);

        while(ldma_ch_get_busy(USART2_DMA_CHANNEL));

        uint16_t usDMAXfersLeft = ldma_ch_get_remaining_xfers(USART2_DMA_CHANNEL); // Number of half words left
        uint32_t ulSize = (USART2_DMA_RX_BUFFER_SIZE >> 1) - (usDMAXfersLeft << 1); // Multiply to get number of bytes

        if(ulSize)
        {
            volatile uint8_t *pubDMANextDst = (volatile uint8_t *)ldma_ch_get_next_dst_addr(USART2_DMA_CHANNEL);
            volatile uint8_t *pubDMABufferReadPos = pubDMANextDst >= (pubUSART2DMABuffer + (USART2_DMA_RX_BUFFER_SIZE >> 1)) ? (pubUSART2DMABuffer + (USART2_DMA_RX_BUFFER_SIZE >> 1)) : pubUSART2DMABuffer;

            while(ulSize--)
            {
                pubUSART2FIFO[usUSART2FIFOWritePos++] = *pubDMABufferReadPos++;

                if(usUSART2FIFOWritePos >= USART2_FIFO_SIZE)
                    usUSART2FIFOWritePos = 0;
            }

        }

        while(USART2->STATUS & USART_STATUS_RXDATAV)
        {
            pubUSART2FIFO[usUSART2FIFOWritePos++] = USART2->RXDATA;

            if(usUSART2FIFOWritePos >= USART2_FIFO_SIZE)
                usUSART2FIFOWritePos = 0;
        }

        ldma_ch_load(USART2_DMA_CHANNEL, pUSART2DMADescriptor);
        ldma_ch_peri_req_enable(USART2_DMA_CHANNEL);
    }
}
static void usart2_dma_isr(uint8_t ubError)
{
    if(ubError)
    {
        ldma_ch_load(USART2_DMA_CHANNEL, pUSART2DMADescriptor);

        return;
    }

    volatile uint8_t *pubDMANextDst = (volatile uint8_t *)ldma_ch_get_next_dst_addr(USART2_DMA_CHANNEL);
    volatile uint8_t *pubDMABufferReadPos = pubDMANextDst >= (pubUSART2DMABuffer + (USART2_DMA_RX_BUFFER_SIZE >> 1)) ? pubUSART2DMABuffer : (pubUSART2DMABuffer + (USART2_DMA_RX_BUFFER_SIZE >> 1)); // If next destination address is on the second buffer it means that the first buffer has just gone full, copy from the first, and vice versa

    uint32_t ulSize = (USART2_DMA_RX_BUFFER_SIZE >> 1);

    while(ulSize--)
    {
        pubUSART2FIFO[usUSART2FIFOWritePos++] = *pubDMABufferReadPos++;

        if(usUSART2FIFOWritePos >= USART2_FIFO_SIZE)
            usUSART2FIFOWritePos = 0;
    }
}

void usart2_init(uint32_t ulBaud, uint32_t ulFrameSettings, int8_t bRXLocation, int8_t bTXLocation, int8_t bCTSLocation, int8_t bRTSLocation)
{
    if(bRXLocation > -1 && bRXLocation > AFCHANLOC_MAX)
        return;

    if(bTXLocation > -1 && bTXLocation > AFCHANLOC_MAX)
        return;

    if(bCTSLocation > -1 && bCTSLocation > AFCHANLOC_MAX)
        return;

    if(bRTSLocation > -1 && bRTSLocation > AFCHANLOC_MAX)
        return;

    cmu_hfper0_clock_gate(CMU_HFPERCLKEN0_USART2, 1);

    USART2->CMD = USART_CMD_CLEARRX | USART_CMD_CLEARTX | USART_CMD_TXTRIDIS | USART_CMD_RXBLOCKDIS | USART_CMD_TXDIS | USART_CMD_RXDIS;

    free((uint8_t *)pubUSART2DMABuffer);
    free((uint8_t *)pubUSART2FIFO);

    pubUSART2DMABuffer = (volatile uint8_t *)malloc(USART2_DMA_RX_BUFFER_SIZE);

    if(!pubUSART2DMABuffer)
        return;

    memset((uint8_t *)pubUSART2DMABuffer, 0, USART2_DMA_RX_BUFFER_SIZE);

    pubUSART2FIFO = (volatile uint8_t *)malloc(USART2_FIFO_SIZE);

    if(!pubUSART2FIFO)
    {
        free((void *)pubUSART2DMABuffer);

        return;
    }

    memset((uint8_t *)pubUSART2FIFO, 0, USART2_FIFO_SIZE);

    usUSART2FIFOWritePos = 0;
    usUSART2FIFOReadPos = 0;

    USART2->CTRL = USART_CTRL_TXBIL_HALFFULL | USART_CTRL_CSMA_NOACTION | USART_CTRL_OVS_X16;
    USART2->CTRLX = (bCTSLocation >= 0 ? USART_CTRLX_CTSEN : 0);
    USART2->FRAME = ulFrameSettings;
    USART2->CLKDIV = (uint32_t)((((float)HFPERB_CLOCK_FREQ / (16.f * ulBaud)) - 1.f) * 256.f);

    USART2->TIMECMP0 = USART_TIMECMP0_TSTOP_RXACT | USART_TIMECMP0_TSTART_RXEOF | 0x08; // RX Timeout after 8 baud times

    USART2->ROUTEPEN = (bRXLocation >= 0 ? USART_ROUTEPEN_RXPEN : 0) | (bTXLocation >= 0 ? USART_ROUTEPEN_TXPEN : 0) | (bCTSLocation >= 0 ? USART_ROUTEPEN_CTSPEN : 0) | (bRTSLocation >= 0 ? USART_ROUTEPEN_RTSPEN : 0);
    USART2->ROUTELOC0 = ((uint32_t)(bRXLocation >= 0 ? bRXLocation : 0) << _USART_ROUTELOC0_RXLOC_SHIFT) | ((uint32_t)(bTXLocation >= 0 ? bTXLocation : 0) << _USART_ROUTELOC0_TXLOC_SHIFT);
    USART2->ROUTELOC1 = ((uint32_t)(bCTSLocation >= 0 ? bCTSLocation : 0) << _USART_ROUTELOC1_CTSLOC_SHIFT) | ((uint32_t)(bRTSLocation >= 0 ? bRTSLocation : 0) << _USART_ROUTELOC1_RTSLOC_SHIFT);

    USART2->IFC = _USART_IFC_MASK; // Clear all flags
    IRQ_CLEAR(USART2_IRQn); // Clear pending vector
    IRQ_SET_PRIO(USART2_IRQn, 2, 0); // Set priority 2,0
    IRQ_ENABLE(USART2_IRQn); // Enable vector
    USART2->IEN |= USART_IEN_TCMP0; // Enable TCMP0 flag

    ldma_ch_disable(USART2_DMA_CHANNEL);
    ldma_ch_peri_req_disable(USART2_DMA_CHANNEL);
    ldma_ch_req_clear(USART2_DMA_CHANNEL);

    ldma_ch_config(USART2_DMA_CHANNEL, LDMA_CH_REQSEL_SOURCESEL_USART2 | LDMA_CH_REQSEL_SIGSEL_USART2RXDATAV, LDMA_CH_CFG_SRCINCSIGN_DEFAULT, LDMA_CH_CFG_DSTINCSIGN_POSITIVE, LDMA_CH_CFG_ARBSLOTS_DEFAULT, 0);
    ldma_ch_set_isr(USART2_DMA_CHANNEL, usart2_dma_isr);

    pUSART2DMADescriptor[0].CTRL = LDMA_CH_CTRL_DSTMODE_ABSOLUTE | LDMA_CH_CTRL_SRCMODE_ABSOLUTE | LDMA_CH_CTRL_DSTINC_ONE | LDMA_CH_CTRL_SIZE_HALFWORD | LDMA_CH_CTRL_SRCINC_NONE | LDMA_CH_CTRL_IGNORESREQ | LDMA_CH_CTRL_REQMODE_BLOCK | LDMA_CH_CTRL_DONEIFSEN | LDMA_CH_CTRL_BLOCKSIZE_UNIT1 | ((((USART2_DMA_RX_BUFFER_SIZE >> 2) - 1) << _LDMA_CH_CTRL_XFERCNT_SHIFT) & _LDMA_CH_CTRL_XFERCNT_MASK) | LDMA_CH_CTRL_STRUCTTYPE_TRANSFER;
    pUSART2DMADescriptor[0].SRC = (void *)&USART2->RXDOUBLE;
    pUSART2DMADescriptor[0].DST = pubUSART2DMABuffer;
    pUSART2DMADescriptor[0].LINK = 0x00000010 | LDMA_CH_LINK_LINK | LDMA_CH_LINK_LINKMODE_RELATIVE;

    pUSART2DMADescriptor[1].CTRL = pUSART2DMADescriptor[0].CTRL;
    pUSART2DMADescriptor[1].SRC = pUSART2DMADescriptor[0].SRC;
    pUSART2DMADescriptor[1].DST = pubUSART2DMABuffer + (USART2_DMA_RX_BUFFER_SIZE >> 1);
    pUSART2DMADescriptor[1].LINK = 0xFFFFFFF0 | LDMA_CH_LINK_LINK | LDMA_CH_LINK_LINKMODE_RELATIVE;

    ldma_ch_load(USART2_DMA_CHANNEL, pUSART2DMADescriptor);
    ldma_ch_peri_req_enable(USART2_DMA_CHANNEL);
    ldma_ch_enable(USART2_DMA_CHANNEL);

    USART2->CMD = (bTXLocation >= 0 ? USART_CMD_TXEN : 0) | (bRXLocation >= 0 ? USART_CMD_RXEN : 0);
}
void usart2_write_byte(const uint8_t ubData)
{
    while(!(USART2->STATUS & USART_STATUS_TXBL));

    USART2->TXDATA = ubData;
}
uint8_t usart2_read_byte()
{
    if(!usart2_available())
        return 0;

    uint8_t ubData;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        ubData = pubUSART2FIFO[usUSART2FIFOReadPos++];

        if(usUSART2FIFOReadPos >= USART2_FIFO_SIZE)
            usUSART2FIFOReadPos = 0;
    }

    return ubData;
}
uint32_t usart2_available()
{
    return (USART2_FIFO_SIZE + usUSART2FIFOWritePos - usUSART2FIFOReadPos) % USART2_FIFO_SIZE;
}
void usart2_flush()
{
    usUSART2FIFOReadPos = usUSART2FIFOWritePos = 0;
}
#endif  // USART2_MODE_SPI

#if defined(USART3_MODE_SPI)
void usart3_init(uint32_t ulBaud, uint8_t ubMode, uint8_t ubBitMode, int8_t bMISOLocation, int8_t bMOSILocation, uint8_t ubCLKLocation)
{
    if(bMISOLocation > -1 && bMISOLocation > AFCHANLOC_MAX)
        return;

    if(bMOSILocation > -1 && bMOSILocation > AFCHANLOC_MAX)
        return;

    if(ubCLKLocation > AFCHANLOC_MAX)
        return;

    cmu_hfper0_clock_gate(CMU_HFPERCLKEN0_USART3, 1);

    USART3->CMD = USART_CMD_CLEARRX | USART_CMD_CLEARTX | USART_CMD_TXTRIDIS | USART_CMD_RXBLOCKDIS | USART_CMD_TXDIS | USART_CMD_RXDIS;

    USART3->CTRL = USART_CTRL_TXBIL_HALFFULL | USART_CTRL_CSMA_NOACTION | (ubBitMode == USART_SPI_MSB_FIRST ? USART_CTRL_MSBF : 0) | (ubMode & 1 ? USART_CTRL_CLKPHA_SAMPLETRAILING : USART_CTRL_CLKPHA_SAMPLELEADING) | (ubMode & 2 ? USART_CTRL_CLKPOL_IDLEHIGH : USART_CTRL_CLKPOL_IDLELOW) | USART_CTRL_SYNC;
    USART3->FRAME = USART_FRAME_DATABITS_EIGHT;
    USART3->CLKDIV = (uint32_t)((((float)HFPER_CLOCK_FREQ / (2.f * ulBaud)) - 1.f) * 256.f);

    USART3->ROUTEPEN = (bMISOLocation >= 0 ? USART_ROUTEPEN_RXPEN : 0) | (bMOSILocation >= 0 ? USART_ROUTEPEN_TXPEN : 0) | USART_ROUTEPEN_CLKPEN;
    USART3->ROUTELOC0 = ((uint32_t)(bMISOLocation >= 0 ? bMISOLocation : 0) << _USART_ROUTELOC0_RXLOC_SHIFT) | ((uint32_t)(bMOSILocation >= 0 ? bMOSILocation : 0) << _USART_ROUTELOC0_TXLOC_SHIFT) | ((uint32_t)ubCLKLocation << _USART_ROUTELOC0_CLKLOC_SHIFT);

    #if defined(USART3_MODE_SPI_3W)
        USART3->CTRL |= USART_CTRL_LOOPBK;
        USART3->CMD = USART_CMD_MASTEREN | (bMOSILocation >= 0 ? (USART_CMD_TXEN | USART_CMD_RXEN) : 0);
    #else
        USART3->CMD = USART_CMD_MASTEREN | (bMOSILocation >= 0 ? USART_CMD_TXEN : 0) | (bMISOLocation >= 0 ? USART_CMD_RXEN : 0);
    #endif  // USART3_MODE_SPI_3W
}
uint8_t usart3_spi_transfer_byte(const uint8_t ubData)
{
    while(!(USART3->STATUS & USART_STATUS_TXIDLE));

    #if defined(USART3_MODE_SPI_3W)
        USART3->CMD = UART_CMD_TXTRIEN;

        while(!(USART3->STATUS & USART_STATUS_TXTRI));
    #endif  // USART3_MODE_SPI_3W

    USART3->CMD = USART_CMD_CLEARRX;

    USART3->TXDATA = ubData;

    while(!(USART3->STATUS & USART_STATUS_TXC));

    return USART3->RXDATA;
}
void usart3_spi_write_byte(const uint8_t ubData, const uint8_t ubWait)
{
    while(!(USART3->STATUS & USART_STATUS_TXBL));

    #if defined(USART3_MODE_SPI_3W)
        USART3->CMD = USART_CMD_TXTRIDIS;

        while(USART3->STATUS & USART_STATUS_TXTRI);
    #endif  // USART3_MODE_SPI_3W

    USART3->TXDATA = ubData;

    while(ubWait && !(USART3->STATUS & USART_STATUS_TXC));
}
#else   // USART3_MODE_SPI
static volatile uint8_t *pubUSART3DMABuffer = NULL;
static volatile uint8_t *pubUSART3FIFO = NULL;
static volatile uint16_t usUSART3FIFOWritePos, usUSART3FIFOReadPos;
static ldma_descriptor_t __attribute__ ((aligned (4))) pUSART3DMADescriptor[2];

void _usart3_isr()
{
    uint32_t ulFlags = USART3->IFC;

    if(ulFlags & USART_IFC_TCMP0)
    {
        ldma_ch_peri_req_disable(USART3_DMA_CHANNEL);

        while(ldma_ch_get_busy(USART3_DMA_CHANNEL));

        uint16_t usDMAXfersLeft = ldma_ch_get_remaining_xfers(USART3_DMA_CHANNEL); // Number of half words left
        uint32_t ulSize = (USART3_DMA_RX_BUFFER_SIZE >> 1) - (usDMAXfersLeft << 1); // Multiply to get number of bytes

        if(ulSize)
        {
            volatile uint8_t *pubDMANextDst = (volatile uint8_t *)ldma_ch_get_next_dst_addr(USART3_DMA_CHANNEL);
            volatile uint8_t *pubDMABufferReadPos = pubDMANextDst >= (pubUSART3DMABuffer + (USART3_DMA_RX_BUFFER_SIZE >> 1)) ? (pubUSART3DMABuffer + (USART3_DMA_RX_BUFFER_SIZE >> 1)) : pubUSART3DMABuffer;

            while(ulSize--)
            {
                pubUSART3FIFO[usUSART3FIFOWritePos++] = *pubDMABufferReadPos++;

                if(usUSART3FIFOWritePos >= USART3_FIFO_SIZE)
                    usUSART3FIFOWritePos = 0;
            }

        }

        while(USART3->STATUS & USART_STATUS_RXDATAV)
        {
            pubUSART3FIFO[usUSART3FIFOWritePos++] = USART3->RXDATA;

            if(usUSART3FIFOWritePos >= USART3_FIFO_SIZE)
                usUSART3FIFOWritePos = 0;
        }

        ldma_ch_load(USART3_DMA_CHANNEL, pUSART3DMADescriptor);
        ldma_ch_peri_req_enable(USART3_DMA_CHANNEL);
    }
}
static void usart3_dma_isr(uint8_t ubError)
{
    if(ubError)
    {
        ldma_ch_load(USART3_DMA_CHANNEL, pUSART3DMADescriptor);

        return;
    }

    volatile uint8_t *pubDMANextDst = (volatile uint8_t *)ldma_ch_get_next_dst_addr(USART3_DMA_CHANNEL);
    volatile uint8_t *pubDMABufferReadPos = pubDMANextDst >= (pubUSART3DMABuffer + (USART3_DMA_RX_BUFFER_SIZE >> 1)) ? pubUSART3DMABuffer : (pubUSART3DMABuffer + (USART3_DMA_RX_BUFFER_SIZE >> 1)); // If next destination address is on the second buffer it means that the first buffer has just gone full, copy from the first, and vice versa

    uint32_t ulSize = (USART3_DMA_RX_BUFFER_SIZE >> 1);

    while(ulSize--)
    {
        pubUSART3FIFO[usUSART3FIFOWritePos++] = *pubDMABufferReadPos++;

        if(usUSART3FIFOWritePos >= USART3_FIFO_SIZE)
            usUSART3FIFOWritePos = 0;
    }
}

void usart3_init(uint32_t ulBaud, uint32_t ulFrameSettings, int8_t bRXLocation, int8_t bTXLocation, int8_t bCTSLocation, int8_t bRTSLocation)
{
    if(bRXLocation > -1 && bRXLocation > AFCHANLOC_MAX)
        return;

    if(bTXLocation > -1 && bTXLocation > AFCHANLOC_MAX)
        return;

    if(bCTSLocation > -1 && bCTSLocation > AFCHANLOC_MAX)
        return;

    if(bRTSLocation > -1 && bRTSLocation > AFCHANLOC_MAX)
        return;

    cmu_hfper0_clock_gate(CMU_HFPERCLKEN0_USART3, 1);

    USART3->CMD = USART_CMD_CLEARRX | USART_CMD_CLEARTX | USART_CMD_TXTRIDIS | USART_CMD_RXBLOCKDIS | USART_CMD_TXDIS | USART_CMD_RXDIS;

    free((uint8_t *)pubUSART3DMABuffer);
    free((uint8_t *)pubUSART3FIFO);

    pubUSART3DMABuffer = (volatile uint8_t *)malloc(USART3_DMA_RX_BUFFER_SIZE);

    if(!pubUSART3DMABuffer)
        return;

    memset((uint8_t *)pubUSART3DMABuffer, 0, USART3_DMA_RX_BUFFER_SIZE);

    pubUSART3FIFO = (volatile uint8_t *)malloc(USART3_FIFO_SIZE);

    if(!pubUSART3FIFO)
    {
        free((void *)pubUSART3DMABuffer);

        return;
    }

    memset((uint8_t *)pubUSART3FIFO, 0, USART3_FIFO_SIZE);

    usUSART3FIFOWritePos = 0;
    usUSART3FIFOReadPos = 0;

    USART3->CTRL = USART_CTRL_TXBIL_HALFFULL | USART_CTRL_CSMA_NOACTION | USART_CTRL_OVS_X16;
    USART3->CTRLX = (bCTSLocation >= 0 ? USART_CTRLX_CTSEN : 0);
    USART3->FRAME = ulFrameSettings;
    USART3->CLKDIV = (uint32_t)((((float)HFPER_CLOCK_FREQ / (16.f * ulBaud)) - 1.f) * 256.f);

    USART3->TIMECMP0 = USART_TIMECMP0_TSTOP_RXACT | USART_TIMECMP0_TSTART_RXEOF | 0x08; // RX Timeout after 8 baud times

    USART3->ROUTEPEN = (bRXLocation >= 0 ? USART_ROUTEPEN_RXPEN : 0) | (bTXLocation >= 0 ? USART_ROUTEPEN_TXPEN : 0) | (bCTSLocation >= 0 ? USART_ROUTEPEN_CTSPEN : 0) | (bRTSLocation >= 0 ? USART_ROUTEPEN_RTSPEN : 0);
    USART3->ROUTELOC0 = ((uint32_t)(bRXLocation >= 0 ? bRXLocation : 0) << _USART_ROUTELOC0_RXLOC_SHIFT) | ((uint32_t)(bTXLocation >= 0 ? bTXLocation : 0) << _USART_ROUTELOC0_TXLOC_SHIFT);
    USART3->ROUTELOC1 = ((uint32_t)(bCTSLocation >= 0 ? bCTSLocation : 0) << _USART_ROUTELOC1_CTSLOC_SHIFT) | ((uint32_t)(bRTSLocation >= 0 ? bRTSLocation : 0) << _USART_ROUTELOC1_RTSLOC_SHIFT);

    USART3->IFC = _USART_IFC_MASK; // Clear all flags
    IRQ_CLEAR(USART3_IRQn); // Clear pending vector
    IRQ_SET_PRIO(USART3_IRQn, 2, 0); // Set priority 2,0
    IRQ_ENABLE(USART3_IRQn); // Enable vector
    USART3->IEN |= USART_IEN_TCMP0; // Enable TCMP0 flag

    ldma_ch_disable(USART3_DMA_CHANNEL);
    ldma_ch_peri_req_disable(USART3_DMA_CHANNEL);
    ldma_ch_req_clear(USART3_DMA_CHANNEL);

    ldma_ch_config(USART3_DMA_CHANNEL, LDMA_CH_REQSEL_SOURCESEL_USART3 | LDMA_CH_REQSEL_SIGSEL_USART3RXDATAV, LDMA_CH_CFG_SRCINCSIGN_DEFAULT, LDMA_CH_CFG_DSTINCSIGN_POSITIVE, LDMA_CH_CFG_ARBSLOTS_DEFAULT, 0);
    ldma_ch_set_isr(USART3_DMA_CHANNEL, usart3_dma_isr);

    pUSART3DMADescriptor[0].CTRL = LDMA_CH_CTRL_DSTMODE_ABSOLUTE | LDMA_CH_CTRL_SRCMODE_ABSOLUTE | LDMA_CH_CTRL_DSTINC_ONE | LDMA_CH_CTRL_SIZE_HALFWORD | LDMA_CH_CTRL_SRCINC_NONE | LDMA_CH_CTRL_IGNORESREQ | LDMA_CH_CTRL_REQMODE_BLOCK | LDMA_CH_CTRL_DONEIFSEN | LDMA_CH_CTRL_BLOCKSIZE_UNIT1 | ((((USART3_DMA_RX_BUFFER_SIZE >> 2) - 1) << _LDMA_CH_CTRL_XFERCNT_SHIFT) & _LDMA_CH_CTRL_XFERCNT_MASK) | LDMA_CH_CTRL_STRUCTTYPE_TRANSFER;
    pUSART3DMADescriptor[0].SRC = (void *)&USART3->RXDOUBLE;
    pUSART3DMADescriptor[0].DST = pubUSART3DMABuffer;
    pUSART3DMADescriptor[0].LINK = 0x00000010 | LDMA_CH_LINK_LINK | LDMA_CH_LINK_LINKMODE_RELATIVE;

    pUSART3DMADescriptor[1].CTRL = pUSART3DMADescriptor[0].CTRL;
    pUSART3DMADescriptor[1].SRC = pUSART3DMADescriptor[0].SRC;
    pUSART3DMADescriptor[1].DST = pubUSART3DMABuffer + (USART3_DMA_RX_BUFFER_SIZE >> 1);
    pUSART3DMADescriptor[1].LINK = 0xFFFFFFF0 | LDMA_CH_LINK_LINK | LDMA_CH_LINK_LINKMODE_RELATIVE;

    ldma_ch_load(USART3_DMA_CHANNEL, pUSART3DMADescriptor);
    ldma_ch_peri_req_enable(USART3_DMA_CHANNEL);
    ldma_ch_enable(USART3_DMA_CHANNEL);

    USART3->CMD = (bTXLocation >= 0 ? USART_CMD_TXEN : 0) | (bRXLocation >= 0 ? USART_CMD_RXEN : 0);
}
void usart3_write_byte(const uint8_t ubData)
{
    while(!(USART3->STATUS & USART_STATUS_TXBL));

    USART3->TXDATA = ubData;
}
uint8_t usart3_read_byte()
{
    if(!usart3_available())
        return 0;

    uint8_t ubData;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        ubData = pubUSART3FIFO[usUSART3FIFOReadPos++];

        if(usUSART3FIFOReadPos >= USART3_FIFO_SIZE)
            usUSART3FIFOReadPos = 0;
    }

    return ubData;
}
uint32_t usart3_available()
{
    return (USART3_FIFO_SIZE + usUSART3FIFOWritePos - usUSART3FIFOReadPos) % USART3_FIFO_SIZE;
}
void usart3_flush()
{
    usUSART3FIFOReadPos = usUSART3FIFOWritePos = 0;
}
#endif  // USART3_MODE_SPI

static volatile uint8_t *pubUART0DMABuffer = NULL;
static volatile uint8_t *pubUART0FIFO = NULL;
static volatile uint16_t usUART0FIFOWritePos, usUART0FIFOReadPos;
static ldma_descriptor_t __attribute__ ((aligned (4))) pUART0DMADescriptor[2];

void _uart0_isr()
{
    uint32_t ulFlags = UART0->IFC;

    if(ulFlags & UART_IFC_TCMP0)
    {
        ldma_ch_peri_req_disable(UART0_DMA_CHANNEL);

        while(ldma_ch_get_busy(UART0_DMA_CHANNEL));

        uint16_t usDMAXfersLeft = ldma_ch_get_remaining_xfers(UART0_DMA_CHANNEL); // Number of half words left
        uint32_t ulSize = (UART0_DMA_RX_BUFFER_SIZE >> 1) - (usDMAXfersLeft << 1); // Multiply to get number of bytes

        if(ulSize)
        {
            volatile uint8_t *pubDMANextDst = (volatile uint8_t *)ldma_ch_get_next_dst_addr(UART0_DMA_CHANNEL);
            volatile uint8_t *pubDMABufferReadPos = pubDMANextDst >= (pubUART0DMABuffer + (UART0_DMA_RX_BUFFER_SIZE >> 1)) ? (pubUART0DMABuffer + (UART0_DMA_RX_BUFFER_SIZE >> 1)) : pubUART0DMABuffer;

            while(ulSize--)
            {
                pubUART0FIFO[usUART0FIFOWritePos++] = *pubDMABufferReadPos++;

                if(usUART0FIFOWritePos >= UART0_FIFO_SIZE)
                    usUART0FIFOWritePos = 0;
            }

        }

        while(UART0->STATUS & UART_STATUS_RXDATAV)
        {
            pubUART0FIFO[usUART0FIFOWritePos++] = UART0->RXDATA;

            if(usUART0FIFOWritePos >= UART0_FIFO_SIZE)
                usUART0FIFOWritePos = 0;
        }

        ldma_ch_load(UART0_DMA_CHANNEL, pUART0DMADescriptor);
        ldma_ch_peri_req_enable(UART0_DMA_CHANNEL);
    }
}
static void uart0_dma_isr(uint8_t ubError)
{
    if(ubError)
    {
        ldma_ch_load(UART0_DMA_CHANNEL, pUART0DMADescriptor);

        return;
    }

    volatile uint8_t *pubDMANextDst = (volatile uint8_t *)ldma_ch_get_next_dst_addr(UART0_DMA_CHANNEL);
    volatile uint8_t *pubDMABufferReadPos = pubDMANextDst >= (pubUART0DMABuffer + (UART0_DMA_RX_BUFFER_SIZE >> 1)) ? pubUART0DMABuffer : (pubUART0DMABuffer + (UART0_DMA_RX_BUFFER_SIZE >> 1)); // If next destination address is on the second buffer it means that the first buffer has just gone full, copy from the first, and vice versa

    uint32_t ulSize = (UART0_DMA_RX_BUFFER_SIZE >> 1);

    while(ulSize--)
    {
        pubUART0FIFO[usUART0FIFOWritePos++] = *pubDMABufferReadPos++;

        if(usUART0FIFOWritePos >= UART0_FIFO_SIZE)
            usUART0FIFOWritePos = 0;
    }
}

void uart0_init(uint32_t ulBaud, uint32_t ulFrameSettings, int8_t bRXLocation, int8_t bTXLocation, int8_t bCTSLocation, int8_t bRTSLocation)
{
    if(bRXLocation > -1 && bRXLocation > AFCHANLOC_MAX)
        return;

    if(bTXLocation > -1 && bTXLocation > AFCHANLOC_MAX)
        return;

    if(bCTSLocation > -1 && bCTSLocation > AFCHANLOC_MAX)
        return;

    if(bRTSLocation > -1 && bRTSLocation > AFCHANLOC_MAX)
        return;

    cmu_hfper1_clock_gate(CMU_HFPERCLKEN1_UART0, 1);

    UART0->CMD = UART_CMD_CLEARRX | UART_CMD_CLEARTX | UART_CMD_TXTRIDIS | UART_CMD_RXBLOCKDIS | UART_CMD_TXDIS | UART_CMD_RXDIS;

    free((uint8_t *)pubUART0DMABuffer);
    free((uint8_t *)pubUART0FIFO);

    pubUART0DMABuffer = (volatile uint8_t *)malloc(UART0_DMA_RX_BUFFER_SIZE);

    if(!pubUART0DMABuffer)
        return;

    memset((uint8_t *)pubUART0DMABuffer, 0, UART0_DMA_RX_BUFFER_SIZE);

    pubUART0FIFO = (volatile uint8_t *)malloc(UART0_FIFO_SIZE);

    if(!pubUART0FIFO)
    {
        free((void *)pubUART0DMABuffer);

        return;
    }

    memset((uint8_t *)pubUART0FIFO, 0, UART0_FIFO_SIZE);

    usUART0FIFOWritePos = 0;
    usUART0FIFOReadPos = 0;

    UART0->CTRL = UART_CTRL_TXBIL_HALFFULL | UART_CTRL_CSMA_NOACTION | UART_CTRL_OVS_X16;
    UART0->CTRLX = (bCTSLocation >= 0 ? UART_CTRLX_CTSEN : 0);
    UART0->FRAME = ulFrameSettings;
    UART0->CLKDIV = (uint32_t)((((float)HFPER_CLOCK_FREQ / (16.f * ulBaud)) - 1.f) * 256.f);

    UART0->TIMECMP0 = UART_TIMECMP0_TSTOP_RXACT | UART_TIMECMP0_TSTART_RXEOF | 0x08; // RX Timeout after 8 baud times

    UART0->ROUTEPEN = (bRXLocation >= 0 ? UART_ROUTEPEN_RXPEN : 0) | (bTXLocation >= 0 ? UART_ROUTEPEN_TXPEN : 0) | (bCTSLocation >= 0 ? UART_ROUTEPEN_CTSPEN : 0) | (bRTSLocation >= 0 ? UART_ROUTEPEN_RTSPEN : 0);
    UART0->ROUTELOC0 = ((uint32_t)(bRXLocation >= 0 ? bRXLocation : 0) << _UART_ROUTELOC0_RXLOC_SHIFT) | ((uint32_t)(bTXLocation >= 0 ? bTXLocation : 0) << _UART_ROUTELOC0_TXLOC_SHIFT);
    UART0->ROUTELOC1 = ((uint32_t)(bCTSLocation >= 0 ? bCTSLocation : 0) << _UART_ROUTELOC1_CTSLOC_SHIFT) | ((uint32_t)(bRTSLocation >= 0 ? bRTSLocation : 0) << _UART_ROUTELOC1_RTSLOC_SHIFT);

    UART0->IFC = _UART_IFC_MASK; // Clear all flags
    IRQ_CLEAR(UART0_IRQn); // Clear pending vector
    IRQ_SET_PRIO(UART0_IRQn, 2, 0); // Set priority 2,0
    IRQ_ENABLE(UART0_IRQn); // Enable vector
    UART0->IEN |= UART_IEN_TCMP0; // Enable TCMP0 flag

    ldma_ch_disable(UART0_DMA_CHANNEL);
    ldma_ch_peri_req_disable(UART0_DMA_CHANNEL);
    ldma_ch_req_clear(UART0_DMA_CHANNEL);

    ldma_ch_config(UART0_DMA_CHANNEL, LDMA_CH_REQSEL_SOURCESEL_UART0 | LDMA_CH_REQSEL_SIGSEL_UART0RXDATAV, LDMA_CH_CFG_SRCINCSIGN_DEFAULT, LDMA_CH_CFG_DSTINCSIGN_POSITIVE, LDMA_CH_CFG_ARBSLOTS_DEFAULT, 0);
    ldma_ch_set_isr(UART0_DMA_CHANNEL, uart0_dma_isr);

    pUART0DMADescriptor[0].CTRL = LDMA_CH_CTRL_DSTMODE_ABSOLUTE | LDMA_CH_CTRL_SRCMODE_ABSOLUTE | LDMA_CH_CTRL_DSTINC_ONE | LDMA_CH_CTRL_SIZE_HALFWORD | LDMA_CH_CTRL_SRCINC_NONE | LDMA_CH_CTRL_IGNORESREQ | LDMA_CH_CTRL_REQMODE_BLOCK | LDMA_CH_CTRL_DONEIFSEN | LDMA_CH_CTRL_BLOCKSIZE_UNIT1 | ((((UART0_DMA_RX_BUFFER_SIZE >> 2) - 1) << _LDMA_CH_CTRL_XFERCNT_SHIFT) & _LDMA_CH_CTRL_XFERCNT_MASK) | LDMA_CH_CTRL_STRUCTTYPE_TRANSFER;
    pUART0DMADescriptor[0].SRC = (void *)&UART0->RXDOUBLE;
    pUART0DMADescriptor[0].DST = pubUART0DMABuffer;
    pUART0DMADescriptor[0].LINK = 0x00000010 | LDMA_CH_LINK_LINK | LDMA_CH_LINK_LINKMODE_RELATIVE;

    pUART0DMADescriptor[1].CTRL = pUART0DMADescriptor[0].CTRL;
    pUART0DMADescriptor[1].SRC = pUART0DMADescriptor[0].SRC;
    pUART0DMADescriptor[1].DST = pubUART0DMABuffer + (UART0_DMA_RX_BUFFER_SIZE >> 1);
    pUART0DMADescriptor[1].LINK = 0xFFFFFFF0 | LDMA_CH_LINK_LINK | LDMA_CH_LINK_LINKMODE_RELATIVE;

    ldma_ch_load(UART0_DMA_CHANNEL, pUART0DMADescriptor);
    ldma_ch_peri_req_enable(UART0_DMA_CHANNEL);
    ldma_ch_enable(UART0_DMA_CHANNEL);

    UART0->CMD = (bTXLocation >= 0 ? UART_CMD_TXEN : 0) | (bRXLocation >= 0 ? UART_CMD_RXEN : 0);
}
void uart0_write_byte(const uint8_t ubData)
{
    while(!(UART0->STATUS & UART_STATUS_TXBL));

    UART0->TXDATA = ubData;
}
uint8_t uart0_read_byte()
{
    if(!uart0_available())
        return 0;

    uint8_t ubData;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        ubData = pubUART0FIFO[usUART0FIFOReadPos++];

        if(usUART0FIFOReadPos >= UART0_FIFO_SIZE)
            usUART0FIFOReadPos = 0;
    }

    return ubData;
}
uint32_t uart0_available()
{
    return (UART0_FIFO_SIZE + usUART0FIFOWritePos - usUART0FIFOReadPos) % UART0_FIFO_SIZE;
}
void uart0_flush()
{
    usUART0FIFOReadPos = usUART0FIFOWritePos = 0;
}