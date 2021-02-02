#include "msc.h"

lock_bits_t *g_psLockBits = (lock_bits_t *)LOCKBITS_BASE;
init_calib_t *g_psInitCalibrationTable = (init_calib_t *)(DEVINFO_BASE & ~(uint32_t)(FLASH_PAGE_SIZE - 1));

void msc_init()
{
    msc_flash_unlock();

    MSC->CTRL |= MSC_CTRL_IFCREADCLEAR | MSC_CTRL_CLKDISFAULTEN | MSC_CTRL_ADDRFAULTEN;
    MSC->READCTRL |= MSC_READCTRL_SCBTP | MSC_READCTRL_PREFETCH;
    MSC->BOOTLOADERCTRL = MSC_BOOTLOADERCTRL_BLWDIS;
    MSC->CACHECONFIG0 = MSC_CACHECONFIG0_CACHELPLEVEL_MINACTIVITY;
    MSC->CACHECMD = MSC_CACHECMD_INVCACHE;

    msc_flash_lock();
}
void msc_config_waitstates(uint32_t ulFrequency)
{
    msc_flash_unlock();

    if(ulFrequency <= 25000000)
        MSC->READCTRL = (MSC->READCTRL & ~_MSC_READCTRL_MODE_MASK) | MSC_READCTRL_MODE_WS0;
    else
        MSC->READCTRL = (MSC->READCTRL & ~_MSC_READCTRL_MODE_MASK) | MSC_READCTRL_MODE_WS1;

    msc_flash_lock();
}
void msc_flash_lock()
{
    MSC->LOCK = MSC_LOCK_LOCKKEY_LOCK;
    MSC->MASSLOCK = MSC_MASSLOCK_LOCKKEY_LOCK;

    MSC->WRITECTRL &= ~MSC_WRITECTRL_WREN;
}
void msc_flash_unlock()
{
    MSC->LOCK = MSC_LOCK_LOCKKEY_UNLOCK;
    MSC->MASSLOCK = MSC_MASSLOCK_LOCKKEY_UNLOCK;

    MSC->WRITECTRL |= MSC_WRITECTRL_WREN;
}
void msc_flash_page_erase(uint32_t ulAddress)
{
    if((ulAddress < FLASH_BASE || ulAddress > FLASH_MEM_END) &&
       (ulAddress < USERDATA_BASE || ulAddress > USERDATA_BASE + FLASH_PAGE_SIZE) &&
       (ulAddress < 0x0FE10000 || ulAddress > 0x0FE10000 + 5 * FLASH_PAGE_SIZE)) // Bootloader
        return;

    ulAddress &= ~(uint32_t)(FLASH_PAGE_SIZE - 1);

    while(MSC->STATUS & MSC_STATUS_BUSY);

    msc_flash_unlock();

    MSC->ADDRB = ulAddress;
    MSC->WRITECMD = MSC_WRITECMD_ERASEPAGE;

    while(MSC->STATUS & MSC_STATUS_BUSY);

    msc_flash_lock();
}
void msc_flash_page_write(uint32_t ulAddress, uint8_t *pubData, uint32_t ulSize)
{
    if(!pubData)
        return;

    if(!ulSize)
        return;

    if((ulAddress < FLASH_BASE || ulAddress > FLASH_MEM_END) &&
       (ulAddress < LOCKBITS_BASE || ulAddress > LOCKBITS_BASE + FLASH_PAGE_SIZE) &&
       (ulAddress < USERDATA_BASE || ulAddress > USERDATA_BASE + FLASH_PAGE_SIZE) &&
       (ulAddress < 0x0FE10000 || ulAddress > 0x0FE10000 + 5 * FLASH_PAGE_SIZE)) // Bootloader
        return;

    if(ulSize & 3) // Only allow full word writes
        return;

    ulAddress &= ~(uint32_t)(FLASH_PAGE_SIZE - 1);

    while(MSC->STATUS & MSC_STATUS_BUSY);

    msc_flash_unlock();

    MSC->ADDRB = ulAddress;
    MSC->WRITECMD = MSC_WRITECMD_LADDRIM;

    for(uint16_t i = 0; i < FLASH_PAGE_SIZE; i += 4)
    {
        if(!ulSize)
            break;

        MSC->WDATA = *(uint32_t *)pubData;
        MSC->WRITECMD = MSC_WRITECMD_WRITEONCE;

        while(MSC->STATUS & MSC_STATUS_BUSY);

        ulSize -= 4;
        ulAddress += 4;
        pubData += 4;
    }

    msc_flash_lock();

    if(!ulSize)
        return;

    msc_flash_page_write(ulAddress, pubData, ulSize);
}
void msc_flash_word_write(uint32_t ulAddress, uint32_t ulData)
{
    if((ulAddress < FLASH_BASE || ulAddress > FLASH_MEM_END) &&
       (ulAddress < LOCKBITS_BASE || ulAddress > LOCKBITS_BASE + FLASH_PAGE_SIZE) &&
       (ulAddress < USERDATA_BASE || ulAddress > USERDATA_BASE + FLASH_PAGE_SIZE) &&
       (ulAddress < 0x0FE10000 || ulAddress > 0x0FE10000 + 5 * FLASH_PAGE_SIZE)) // Bootloader
        return;

    if(ulAddress & 3) // Only allow aligned writes
        return;

    while(MSC->STATUS & MSC_STATUS_BUSY);

    msc_flash_unlock();

    MSC->ADDRB = ulAddress;
    MSC->WRITECMD = MSC_WRITECMD_LADDRIM;
    MSC->WDATA = ulData;
    MSC->WRITECMD = MSC_WRITECMD_WRITEONCE;

    while(MSC->STATUS & MSC_STATUS_BUSY);

    msc_flash_lock();
}