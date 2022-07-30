#include "ublox.h"

static uint8_t ublox_write_cmd(uint8_t *pubCommand, uint32_t ulCommandSize)
{
    if(!pubCommand || ulCommandSize < 2)
        return 0;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
        return i2c2_write(UBLOX_I2C_ADDR, pubCommand, ulCommandSize, I2C_STOP);
}
static uint16_t ublox_bytes_available()
{
    uint16_t usBytesAvailable = 0;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        i2c2_write_byte(UBLOX_I2C_ADDR, 0xFD, I2C_RESTART);
        i2c2_read(UBLOX_I2C_ADDR, (uint8_t *)&usBytesAvailable, sizeof(uint16_t), I2C_STOP);
    }

    return (usBytesAvailable >> 8) | (usBytesAvailable << 8);
}
static uint8_t ublox_read(uint8_t *pubBuffer, uint32_t ulBufferSize)
{
    if(!pubBuffer || !ulBufferSize)
        return 0;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        i2c2_write_byte(UBLOX_I2C_ADDR, 0xFF, I2C_RESTART);
        return i2c2_read(UBLOX_I2C_ADDR, pubBuffer, ulBufferSize, I2C_STOP);
    }
}

static uint16_t ublox_calc_checksum(const uint8_t *pubBuffer, uint32_t ulBufferSize)
{
    uint8_t ubCA = 0;
    uint8_t ubCB = 0;

    for(uint32_t i = 0; i < ulBufferSize; i++)
    {
        ubCA += pubBuffer[i];
        ubCB += ubCA;
    }

    return ((uint16_t)ubCB << 8) | ubCA;
}
static uint8_t *ublox_build_packet(uint8_t ubClass, uint8_t ubID, const uint8_t *pubPayload, uint16_t usPayloadSize, uint32_t *pulPacketSize)
{
    uint8_t *pubPacket = (uint8_t *)malloc(usPayloadSize + 8);

    if(!pubPacket)
        return NULL;

    pubPacket[0] = 0xB5;
    pubPacket[1] = 0x62;
    pubPacket[2] = ubClass;
    pubPacket[3] = ubID;
    pubPacket[4] = (uint8_t)((usPayloadSize >> 0) & 0xFF);
    pubPacket[5] = (uint8_t)((usPayloadSize >> 8) & 0xFF);

    if(pubPayload && usPayloadSize)
        memcpy(pubPacket + 6, pubPayload, usPayloadSize);

    uint16_t usChecksum = ublox_calc_checksum(pubPacket + 2, usPayloadSize + 4);

    pubPacket[usPayloadSize + 6] = (uint8_t)((usChecksum >> 0) & 0xFF);
    pubPacket[usPayloadSize + 7] = (uint8_t)((usChecksum >> 8) & 0xFF);

    if(pulPacketSize)
        *pulPacketSize = usPayloadSize + 8;

    return pubPacket;
}

uint8_t ublox_init()
{
    if(!i2c2_write(UBLOX_I2C_ADDR, NULL, 0, I2C_STOP)) // Check ACK from the expected address
        return 0;

    GPS_RESET();
    delay_ms(50);
    GPS_UNRESET();
    delay_ms(100);

    ////
    const uint8_t DIS_NMEA[] = {
        //0xB5, 0x62,
        //0x06,
        //0x00,
        //0x14, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
        //0xA0, 0x96
    };

    uint32_t plen;
    uint8_t *pkt = ublox_build_packet(0x06, 0x00, DIS_NMEA, sizeof(DIS_NMEA), &plen);

    ublox_write_cmd(pkt, plen);
    free(pkt);

    ////
    uint8_t pid = 1;
    pkt = ublox_build_packet(0x06, 0x00, &pid, 1, &plen);

    ublox_write_cmd(pkt, plen);
    free(pkt);

    ////
    pkt = ublox_build_packet(0x06, 0x31, NULL, 0, &plen);

    ublox_write_cmd(pkt, plen);
    free(pkt);

    // TODO: Work

    return 1;
}
void ublox_isr()
{

}

void ublox_poll()
{
    //
    uint32_t plen;
    uint8_t *pkt = ublox_build_packet(0x01, 0x07, NULL, 0, &plen);

    //ublox_write_cmd(pkt, plen);
    free(pkt);
    //

    uint16_t usBytesAvailable = ublox_bytes_available();

    if(!usBytesAvailable)
        return;

    uint8_t *pubBuffer = (uint8_t *)malloc(usBytesAvailable);

    if(!pubBuffer)
        return;

    ublox_read(pubBuffer, usBytesAvailable);

    if(pubBuffer[0] != 0xB5 || pubBuffer[1] != 0x62)
    {
        free(pubBuffer);

        return;
    }

    #include "debug_macros.h"
    DBGPRINT_CTX("ublox pkt %hu: [", usBytesAvailable);
    for(uint32_t i = 0; i < usBytesAvailable; i++)
        DBGPRINT("%02X ", pubBuffer[i]);
    DBGPRINTLN("]");

    if(pubBuffer[2] == 0x01 && pubBuffer[3] == 0x07)
    {
        uint16_t usSize = (pubBuffer[5] << 8) | pubBuffer[4];

        if(usSize == 0x005C)
        {
            uint32_t ulITOW = (uint32_t)(pubBuffer[6] << 0) | (uint32_t)(pubBuffer[7] << 8) | (uint32_t)(pubBuffer[8] << 16) | (uint32_t)(pubBuffer[9] << 24);
            uint16_t usYear = (uint16_t)(pubBuffer[10] << 0) | (uint16_t)(pubBuffer[11] << 8);
            uint8_t ubMonth = pubBuffer[12];
            uint8_t ubDay = pubBuffer[13];
            uint8_t ubHour = pubBuffer[14];
            uint8_t ubMinute = pubBuffer[15];
            uint8_t ubSecond = pubBuffer[16];
            uint8_t ubValid = pubBuffer[17];
            uint32_t ulTAcc = (uint32_t)(pubBuffer[18] << 0) | (uint32_t)(pubBuffer[19] << 8) | (uint32_t)(pubBuffer[20] << 16) | (uint32_t)(pubBuffer[21] << 24);
            int32_t iNano = (int32_t)(pubBuffer[22] << 0) | (int32_t)(pubBuffer[23] << 8) | (int32_t)(pubBuffer[24] << 16) | (int32_t)(pubBuffer[25] << 24);
            uint8_t ubFixType = pubBuffer[26];
            uint8_t ubFixFlags = pubBuffer[27];
            uint8_t ubFixFlags2 = pubBuffer[28];
            uint8_t ubNumSv = pubBuffer[29];
            int32_t iLon = (int32_t)(pubBuffer[30] << 0) | (int32_t)(pubBuffer[31] << 8) | (int32_t)(pubBuffer[32] << 16) | (int32_t)(pubBuffer[33] << 24);
            int32_t iLat = (int32_t)(pubBuffer[34] << 0) | (int32_t)(pubBuffer[35] << 8) | (int32_t)(pubBuffer[36] << 16) | (int32_t)(pubBuffer[37] << 24);
            int32_t iHeight = (int32_t)(pubBuffer[38] << 0) | (int32_t)(pubBuffer[39] << 8) | (int32_t)(pubBuffer[40] << 16) | (int32_t)(pubBuffer[41] << 24);
            int32_t iHeightAboveMSL = (int32_t)(pubBuffer[42] << 0) | (int32_t)(pubBuffer[43] << 8) | (int32_t)(pubBuffer[44] << 16) | (int32_t)(pubBuffer[45] << 24);
            uint32_t ulHAcc = (uint32_t)(pubBuffer[46] << 0) | (uint32_t)(pubBuffer[47] << 8) | (uint32_t)(pubBuffer[48] << 16) | (uint32_t)(pubBuffer[49] << 24);
            uint32_t ulVAcc = (uint32_t)(pubBuffer[50] << 0) | (uint32_t)(pubBuffer[51] << 8) | (uint32_t)(pubBuffer[52] << 16) | (uint32_t)(pubBuffer[53] << 24);
            int32_t iNEDVelN = (int32_t)(pubBuffer[54] << 0) | (int32_t)(pubBuffer[55] << 8) | (int32_t)(pubBuffer[56] << 16) | (int32_t)(pubBuffer[57] << 24);
            int32_t iNEDVelE = (int32_t)(pubBuffer[58] << 0) | (int32_t)(pubBuffer[59] << 8) | (int32_t)(pubBuffer[60] << 16) | (int32_t)(pubBuffer[61] << 24);
            int32_t iNEDVelD = (int32_t)(pubBuffer[62] << 0) | (int32_t)(pubBuffer[63] << 8) | (int32_t)(pubBuffer[64] << 16) | (int32_t)(pubBuffer[65] << 24);
            int32_t iGSpeed = (int32_t)(pubBuffer[66] << 0) | (int32_t)(pubBuffer[67] << 8) | (int32_t)(pubBuffer[68] << 16) | (int32_t)(pubBuffer[69] << 24);
            int32_t iHeading = (int32_t)(pubBuffer[70] << 0) | (int32_t)(pubBuffer[71] << 8) | (int32_t)(pubBuffer[72] << 16) | (int32_t)(pubBuffer[73] << 24);
            uint32_t ulSpeedAcc = (uint32_t)(pubBuffer[74] << 0) | (uint32_t)(pubBuffer[75] << 8) | (uint32_t)(pubBuffer[76] << 16) | (uint32_t)(pubBuffer[77] << 24);
            uint32_t ulHeadingAcc = (uint32_t)(pubBuffer[78] << 0) | (uint32_t)(pubBuffer[79] << 8) | (uint32_t)(pubBuffer[80] << 16) | (uint32_t)(pubBuffer[81] << 24);
            uint16_t usPDOP = (uint16_t)(pubBuffer[82] << 0) | (uint16_t)(pubBuffer[83] << 8);
            uint16_t usFixFlags3 = (uint16_t)(pubBuffer[84] << 0) | (uint16_t)(pubBuffer[85] << 8);
            int32_t iHeadingVehicle = (int32_t)(pubBuffer[90] << 0) | (int32_t)(pubBuffer[91] << 8) | (int32_t)(pubBuffer[92] << 16) | (int32_t)(pubBuffer[93] << 24);
            int16_t iMagDec = (int16_t)(pubBuffer[94] << 0) | (int16_t)(pubBuffer[95] << 8);
            int16_t iMagDecAcc = (int16_t)(pubBuffer[96] << 0) | (int16_t)(pubBuffer[97] << 8);

            DBGPRINTLN_CTX("iTOW: %u", ulITOW);
            DBGPRINTLN_CTX("usYear: %u", usYear);
            DBGPRINTLN_CTX("ubMonth: %u", ubMonth);
            DBGPRINTLN_CTX("ubDay: %u", ubDay);
            DBGPRINTLN_CTX("ubHour: %u", ubHour);
            DBGPRINTLN_CTX("ubMinute: %u", ubMinute);
            DBGPRINTLN_CTX("ubSecond: %u", ubSecond);
            DBGPRINTLN_CTX("ubValid: %u", ubValid);
            DBGPRINTLN_CTX("ulTAcc: %u", ulTAcc);
            DBGPRINTLN_CTX("iNano: %d", iNano);
            DBGPRINTLN_CTX("ubFixType: %u", ubFixType);
            DBGPRINTLN_CTX("ubFixFlags: %u", ubFixFlags);
            DBGPRINTLN_CTX("ubFixFlags2: %u", ubFixFlags2);
            DBGPRINTLN_CTX("ubNumSv: %u", ubNumSv);

            DBGPRINTLN_CTX("Longitude: %.6f deg", (double)iLon * 1e-7);
            DBGPRINTLN_CTX("Latitude: %.6f deg", (double)iLat * 1e-7);
            DBGPRINTLN_CTX("Altitude: %.6f m", (double)iHeightAboveMSL * 1e-3);
        }

    }

    free(pubBuffer);
}
