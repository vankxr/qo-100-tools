#include "ssd1306.h"

static uint8_t pubFrameBuffer[SSD1306_BUFFER_SIZE];
static const uint8_t pubFontData[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, // (space)
    0x00, 0x00, 0x5F, 0x00, 0x00, // !
    0x00, 0x07, 0x00, 0x07, 0x00, // "
    0x14, 0x7F, 0x14, 0x7F, 0x14, // #
    0x24, 0x2A, 0x7F, 0x2A, 0x12, // $
    0x23, 0x13, 0x08, 0x64, 0x62, // %
    0x36, 0x49, 0x55, 0x22, 0x50, // &
    0x00, 0x05, 0x03, 0x00, 0x00, // '
    0x00, 0x1C, 0x22, 0x41, 0x00, // (
    0x00, 0x41, 0x22, 0x1C, 0x00, // )
    0x08, 0x2A, 0x1C, 0x2A, 0x08, // *
    0x08, 0x08, 0x3E, 0x08, 0x08, // +
    0x00, 0x50, 0x30, 0x00, 0x00, // ,
    0x08, 0x08, 0x08, 0x08, 0x08, // -
    0x00, 0x60, 0x60, 0x00, 0x00, // .
    0x20, 0x10, 0x08, 0x04, 0x02, // /
    0x3E, 0x51, 0x49, 0x45, 0x3E, // 0
    0x00, 0x42, 0x7F, 0x40, 0x00, // 1
    0x42, 0x61, 0x51, 0x49, 0x46, // 2
    0x21, 0x41, 0x45, 0x4B, 0x31, // 3
    0x18, 0x14, 0x12, 0x7F, 0x10, // 4
    0x27, 0x45, 0x45, 0x45, 0x39, // 5
    0x3C, 0x4A, 0x49, 0x49, 0x30, // 6
    0x01, 0x71, 0x09, 0x05, 0x03, // 7
    0x36, 0x49, 0x49, 0x49, 0x36, // 8
    0x06, 0x49, 0x49, 0x29, 0x1E, // 9
    0x00, 0x36, 0x36, 0x00, 0x00, // :
    0x00, 0x56, 0x36, 0x00, 0x00, // ;
    0x00, 0x08, 0x14, 0x22, 0x41, // <
    0x14, 0x14, 0x14, 0x14, 0x14, // =
    0x41, 0x22, 0x14, 0x08, 0x00, // >
    0x02, 0x01, 0x51, 0x09, 0x06, // ?
    0x32, 0x49, 0x79, 0x41, 0x3E, // @
    0x7E, 0x11, 0x11, 0x11, 0x7E, // A
    0x7F, 0x49, 0x49, 0x49, 0x36, // B
    0x3E, 0x41, 0x41, 0x41, 0x22, // C
    0x7F, 0x41, 0x41, 0x22, 0x1C, // D
    0x7F, 0x49, 0x49, 0x49, 0x41, // E
    0x7F, 0x09, 0x09, 0x01, 0x01, // F
    0x3E, 0x41, 0x41, 0x51, 0x32, // G
    0x7F, 0x08, 0x08, 0x08, 0x7F, // H
    0x00, 0x41, 0x7F, 0x41, 0x00, // I
    0x20, 0x40, 0x41, 0x3F, 0x01, // J
    0x7F, 0x08, 0x14, 0x22, 0x41, // K
    0x7F, 0x40, 0x40, 0x40, 0x40, // L
    0x7F, 0x02, 0x04, 0x02, 0x7F, // M
    0x7F, 0x04, 0x08, 0x10, 0x7F, // N
    0x3E, 0x41, 0x41, 0x41, 0x3E, // O
    0x7F, 0x09, 0x09, 0x09, 0x06, // P
    0x3E, 0x41, 0x51, 0x21, 0x5E, // Q
    0x7F, 0x09, 0x19, 0x29, 0x46, // R
    0x46, 0x49, 0x49, 0x49, 0x31, // S
    0x01, 0x01, 0x7F, 0x01, 0x01, // T
    0x3F, 0x40, 0x40, 0x40, 0x3F, // U
    0x1F, 0x20, 0x40, 0x20, 0x1F, // V
    0x7F, 0x20, 0x18, 0x20, 0x7F, // W
    0x63, 0x14, 0x08, 0x14, 0x63, // X
    0x03, 0x04, 0x78, 0x04, 0x03, // Y
    0x61, 0x51, 0x49, 0x45, 0x43, // Z
    0x00, 0x00, 0x7F, 0x41, 0x41, // [
    0x02, 0x04, 0x08, 0x10, 0x20, // "\"
    0x41, 0x41, 0x7F, 0x00, 0x00, // ]
    0x04, 0x02, 0x01, 0x02, 0x04, // ^
    0x40, 0x40, 0x40, 0x40, 0x40, // _
    0x00, 0x01, 0x02, 0x04, 0x00, // `
    0x20, 0x54, 0x54, 0x54, 0x78, // a
    0x7F, 0x48, 0x44, 0x44, 0x38, // b
    0x38, 0x44, 0x44, 0x44, 0x20, // c
    0x38, 0x44, 0x44, 0x48, 0x7F, // d
    0x38, 0x54, 0x54, 0x54, 0x18, // e
    0x08, 0x7E, 0x09, 0x01, 0x02, // f
    0x08, 0x14, 0x54, 0x54, 0x3C, // g
    0x7F, 0x08, 0x04, 0x04, 0x78, // h
    0x00, 0x44, 0x7D, 0x40, 0x00, // i
    0x20, 0x40, 0x44, 0x3D, 0x00, // j
    0x00, 0x7F, 0x10, 0x28, 0x44, // k
    0x00, 0x41, 0x7F, 0x40, 0x00, // l
    0x7C, 0x04, 0x18, 0x04, 0x78, // m
    0x7C, 0x08, 0x04, 0x04, 0x78, // n
    0x38, 0x44, 0x44, 0x44, 0x38, // o
    0x7C, 0x14, 0x14, 0x14, 0x08, // p
    0x08, 0x14, 0x14, 0x18, 0x7C, // q
    0x7C, 0x08, 0x04, 0x04, 0x08, // r
    0x48, 0x54, 0x54, 0x54, 0x20, // s
    0x04, 0x3F, 0x44, 0x40, 0x20, // t
    0x3C, 0x40, 0x40, 0x20, 0x7C, // u
    0x1C, 0x20, 0x40, 0x20, 0x1C, // v
    0x3C, 0x40, 0x30, 0x40, 0x3C, // w
    0x44, 0x28, 0x10, 0x28, 0x44, // x
    0x0C, 0x50, 0x50, 0x50, 0x3C, // y
    0x44, 0x64, 0x54, 0x4C, 0x44, // z
    0x00, 0x08, 0x36, 0x41, 0x00, // {
    0x00, 0x00, 0x7F, 0x00, 0x00, // |
    0x00, 0x41, 0x36, 0x08, 0x00, // }
    0x06, 0x09, 0x09, 0x09, 0x06, // º
    0x08, 0x08, 0x2A, 0x1C, 0x08, // ->
    0x08, 0x1C, 0x2A, 0x08, 0x08  // <-
};
static const uint8_t pubFontLookup[] = {
    ' ', '!', '"', '#', '$', '%', '&', '\'', '(', ')',
    '*', '+', ',', '-', '.', '/',
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
    ':', ';', '<', '=', '>', '?', '@',
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
    '[', '\\', ']', '^', '_', '`',
    'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
    '{', '|', '}', 186 /* º */
};

static uint8_t ssd1306_read()
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        return i2c2_read_byte(SSD1306_I2C_ADDR, I2C_STOP);
    }
}
static void ssd1306_write(uint8_t ubCommand, uint8_t *pubData, uint16_t usSize)
{
    uint8_t *pubBuffer = (uint8_t*)malloc(usSize + 1);

    if(!pubBuffer)
        return;

    memcpy(pubBuffer + 1, pubData, usSize);

    pubBuffer[0] = ubCommand ? 0x00 : 0x40;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        i2c2_write(SSD1306_I2C_ADDR, pubBuffer, usSize + 1, I2C_STOP);
    }

    free(pubBuffer);
}

uint8_t ssd1306_init()
{
    if(!i2c2_write(SSD1306_I2C_ADDR, NULL, 0, I2C_STOP)) // Check ACK from the expected address
        return 0;

    uint8_t pubInitSeq[] = SSD1306_INIT_SEQUENCE;

    ssd1306_write(1, pubInitSeq, sizeof(pubInitSeq));

    ssd1306_clear_display(0);
    ssd1306_set_scroll(0, 0, 0);
    ssd1306_update();

    return 1;
}

void ssd1306_draw_pixel(uint8_t ubX, uint8_t ubY, uint8_t ubColor, uint8_t ubUpdate)
{
    if(ubX > SSD1306_WIDTH || ubY > SSD1306_HEIGHT)
        return;

    uint8_t ubPage = ubY / 8;
    uint16_t ubByte = (ubPage == 0) ? ubX : ubX + (SSD1306_WIDTH * ubPage);

    if(ubColor)
        pubFrameBuffer[ubByte] |= (1 << (ubY - 8 * ubPage));
    else
        pubFrameBuffer[ubByte] &= ~(1 << (ubY - 8 * ubPage));

    if(ubUpdate)
        ssd1306_update();
}
void ssd1306_draw_line(uint8_t ubX0, uint8_t ubY0, uint8_t ubX1, uint8_t ubY1, uint8_t ubColor, uint8_t ubUpdate)
{
    uint8_t dx = abs(ubX1 - ubX0);
    uint8_t sx = (ubX0 < ubX1) ? 1 : -1;
    uint8_t dy = abs(ubY1 - ubY0);
    uint8_t sy = (ubY0 < ubY1) ? 1 : -1;

    float err = ((dx > dy) ? dx : -dy) / 2;

    while(ubX0 != ubX1 || ubY0 != ubY1)
    {
        ssd1306_draw_pixel(ubX0, ubY0, ubColor, 0);

        float _err = err;

        if(_err > -dx)
        {
            err -= dy;
            ubX0 += sx;
        }

        if(_err < dy)
        {
            err += dx;
            ubY0 += sy;
        }
    }

    if(ubUpdate)
        ssd1306_update();
}
void ssd1306_draw_rect(uint8_t ubX, uint8_t ubY, uint8_t ubWidth, uint8_t ubHeight, uint8_t ubFill, uint8_t ubColor, uint8_t ubUpdate)
{
    if(ubFill)
    {
        for(uint8_t i = ubX; i < ubX + ubWidth; i++)
            ssd1306_draw_line(i, ubY, i, ubY + ubHeight - 1, ubColor, 0);
    }
    else
    {
        ssd1306_draw_line(ubX, ubY, ubX + ubWidth, ubY, ubColor, 0);
        ssd1306_draw_line(ubX, ubY, ubX, ubY + ubHeight, ubColor, 0);
        ssd1306_draw_line(ubX + ubWidth, ubY, ubX + ubWidth, ubY + ubHeight, ubColor, 0);
        ssd1306_draw_line(ubX, ubY + ubHeight, ubX + ubWidth, ubY + ubHeight, ubColor, 0);
    }

    if(ubUpdate)
        ssd1306_update();
}
void ssd1306_draw_circle(uint8_t ubX, uint8_t ubY, uint8_t ubRadius, uint8_t ubColor, uint8_t ubUpdate)
{
    uint8_t f = 1 - ubRadius;
    uint8_t ddF_x = 1;
    uint8_t ddF_y = -2 * ubRadius;
    uint8_t xbuf = 0;
    uint8_t ybuf = ubRadius;

    ssd1306_draw_pixel(ubX, ubY + ubRadius, ubColor, 0);
    ssd1306_draw_pixel(ubX, ubY - ubRadius, ubColor, 0);
    ssd1306_draw_pixel(ubX + ubRadius, ubY, ubColor, 0);
    ssd1306_draw_pixel(ubX - ubRadius, ubY, ubColor, 0);

    while(ubX < ubY)
    {
        if (f >= 0)
        {
            ybuf--;
            ddF_y += 2;
            f += ddF_y;
        }

        xbuf++;
        ddF_x += 2;
        f += ddF_x;

        ssd1306_draw_pixel(ubX + xbuf, ubY + ybuf, ubColor, 0);
        ssd1306_draw_pixel(ubX - xbuf, ubY + ybuf, ubColor, 0);
        ssd1306_draw_pixel(ubX + xbuf, ubY - ybuf, ubColor, 0);
        ssd1306_draw_pixel(ubX - xbuf, ubY - ybuf, ubColor, 0);
        ssd1306_draw_pixel(ubX + ybuf, ubY + xbuf, ubColor, 0);
        ssd1306_draw_pixel(ubX - ybuf, ubY + xbuf, ubColor, 0);
        ssd1306_draw_pixel(ubX + ybuf, ubY - xbuf, ubColor, 0);
        ssd1306_draw_pixel(ubX - ybuf, ubY - xbuf, ubColor, 0);
    }

    if(ubUpdate)
        ssd1306_update();
}
void ssd1306_draw_string(uint8_t ubX, uint8_t ubY, const char* pszString, uint8_t ubColor, uint8_t ubUpdate)
{
    uint8_t xbuf = ubX;
    uint8_t ubPage = ubY / 8;

    for(uint8_t i = 0; pszString[i] != 0x00; i++)
    {
        if(xbuf + 5 > SSD1306_WIDTH || ubY + 7 > SSD1306_HEIGHT)
            return;

        uint16_t usLoc = 0;

        for(uint16_t j = 0; j < sizeof(pubFontLookup); j++)
        {
            if(pubFontLookup[j] == pszString[i])
            {
                usLoc = j;

                break;
            }
        }

        if(pubFontLookup[usLoc] == pszString[i])
        {
            uint16_t usByte = xbuf + (SSD1306_WIDTH * ubPage);

            if(usByte + 4 >= SSD1306_BUFFER_SIZE)
                return;

            usLoc *= 5;

            for(uint8_t j = 0; j < 5; j++)
            {
                pubFrameBuffer[usByte + j] = pubFontData[usLoc + j];

                if(!ubColor)
                    pubFrameBuffer[usByte + j] = ~pubFrameBuffer[usByte + j];
            }

            if(usLoc != 0)
            {
                xbuf += 6;

                pubFrameBuffer[xbuf - 1 + (SSD1306_WIDTH * ubPage)] = 0x00;
            }
            else
            {
                xbuf += 5;
            }
        }
    }

    if(ubUpdate)
        ssd1306_update();
}

void ssd1306_set_scroll(uint8_t ubDirection, uint8_t ubStart, uint8_t ubStop)
{
    if(!ubStart && !ubStop)
    {
        uint8_t ubData = SSD1306_DEACTIVATE_SCROLL;

        ssd1306_write(1, &ubData, sizeof(ubData));

        return;
    }

    uint8_t ubData[] = {ubDirection ? SSD1306_RIGHT_HORIZONTAL_SCROLL : SSD1306_LEFT_HORIZONTAL_SCROLL, 0x00, ubStart, 0x00, ubStop, 0x00, 0xFF, SSD1306_ACTIVATE_SCROLL};

    ssd1306_write(1, ubData, sizeof(ubData));
}

void ssd1306_set_brightness(uint8_t ubBrightness)
{
    uint8_t ubData[] = {SSD1306_SET_CONTRAST, ubBrightness ? 0xCF : 0x00};

    ssd1306_write(1, ubData, sizeof(ubData));
}
void ssd1306_set_status(uint8_t ubStatus)
{
    uint8_t ubData = ubStatus ? SSD1306_DISPLAY_ON : SSD1306_DISPLAY_OFF;

    ssd1306_write(1, &ubData, sizeof(ubData));
}
void ssd1306_set_inverted(uint8_t ubInverted)
{
    uint8_t ubData = ubInverted ? SSD1306_INVERT_DISPLAY : SSD1306_NORMAL_DISPLAY;

    ssd1306_write(1, &ubData, sizeof(ubData));
}

void ssd1306_clear_display(uint8_t ubUpdate)
{
    memset(pubFrameBuffer, 0, SSD1306_BUFFER_SIZE);

    if(ubUpdate)
        ssd1306_update();
}
void ssd1306_update()
{
    ssd1306_ready_wait();

    uint8_t pubDisplaySeq[] = SSD1306_DISPLAY_SEQUENCE;

    ssd1306_write(1, pubDisplaySeq, sizeof(pubDisplaySeq));
    ssd1306_write(0, pubFrameBuffer, sizeof(pubFrameBuffer));
}
void ssd1306_ready_wait()
{
    while(ssd1306_read() & 0x80)
        delay_ms(1);
}
