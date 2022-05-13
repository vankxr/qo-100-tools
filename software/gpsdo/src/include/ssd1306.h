#ifndef __SSD1306_H__
#define __SSD1306_H__

#include <em_device.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "systick.h"
#include "atomic.h"
#include "i2c.h"

#define SSD1306_I2C_ADDR 0x3C

// Constants
#define SSD1306_WIDTH       128
#define SSD1306_HEIGHT      32
#define SSD1306_BUFFER_SIZE (SSD1306_WIDTH * SSD1306_HEIGHT) / 8

#define SSD1306_MULTIPLEX       0x1F
#define SSD1306_COM_PINS        0x02
#define SSD1306_COLUMN_OFFSET   0

// Commands
#define SSD1306_DISPLAY_OFF                             0xAE
#define SSD1306_DISPLAY_ON                              0xAF
#define SSD1306_SET_DISPLAY_CLOCK_DIV                   0xD5
#define SSD1306_SET_MULTIPLEX                           0xA8
#define SSD1306_SET_DISPLAY_OFFSET                      0xD3
#define SSD1306_SET_START_LINE                          0x00
#define SSD1306_CHARGE_PUMP                             0x8D
#define SSD1306_MEMORY_MODE                             0x20
#define SSD1306_SEG_REMAP                               0xA1 // using 0xA0 will flip screen
#define SSD1306_COM_SCAN_DEC                            0xC8
#define SSD1306_COM_SCAN_INC                            0xC0
#define SSD1306_SET_COM_PINS                            0xDA
#define SSD1306_SET_CONTRAST                            0x81
#define SSD1306_SET_PRECHARGE                           0xD9
#define SSD1306_SET_VCOM_DETECT                         0xDB
#define SSD1306_DISPLAY_ALL_ON_RESUME                   0xA4
#define SSD1306_NORMAL_DISPLAY                          0xA6
#define SSD1306_COLUMN_ADDR                             0x21
#define SSD1306_PAGE_ADDR                               0x22
#define SSD1306_INVERT_DISPLAY                          0xA7
#define SSD1306_ACTIVATE_SCROLL                         0x2F
#define SSD1306_DEACTIVATE_SCROLL                       0x2E
#define SSD1306_SET_VERTICAL_SCROLL_AREA                0xA3
#define SSD1306_RIGHT_HORIZONTAL_SCROLL                 0x26
#define SSD1306_LEFT_HORIZONTAL_SCROLL                  0x27
#define SSD1306_VERTICAL_AND_RIGHT_HORIZONTAL_SCROLL    0x29
#define SSD1306_VERTICAL_AND_LEFT_HORIZONTAL_SCROLL     0x2A

#define SSD1306_INIT_SEQUENCE { \
    SSD1306_DISPLAY_OFF, \
    SSD1306_SET_DISPLAY_CLOCK_DIV, 0x80, \
    SSD1306_SET_MULTIPLEX, SSD1306_MULTIPLEX, \
    SSD1306_SET_DISPLAY_OFFSET, 0x00, \
    SSD1306_SET_START_LINE, \
    SSD1306_CHARGE_PUMP, 0x14, \
    SSD1306_MEMORY_MODE, 0x00, \
    SSD1306_SEG_REMAP, \
    SSD1306_COM_SCAN_DEC, \
    SSD1306_SET_COM_PINS, SSD1306_COM_PINS, \
    SSD1306_SET_CONTRAST, 0xCF, \
    SSD1306_SET_PRECHARGE, 0xF1, \
    SSD1306_SET_VCOM_DETECT, 0x40, \
    SSD1306_DISPLAY_ALL_ON_RESUME, \
    SSD1306_NORMAL_DISPLAY, \
    SSD1306_DISPLAY_ON \
}
#define SSD1306_DISPLAY_SEQUENCE { \
    SSD1306_COLUMN_ADDR, \
    SSD1306_COLUMN_OFFSET, \
    SSD1306_COLUMN_OFFSET + SSD1306_WIDTH - 1, \
    SSD1306_PAGE_ADDR, 0, (SSD1306_HEIGHT / 8) - 1 \
}

uint8_t ssd1306_init();

void ssd1306_draw_pixel(uint8_t ubX, uint8_t ubY, uint8_t ubColor, uint8_t ubUpdate);
void ssd1306_draw_line(uint8_t ubX0, uint8_t ubY0, uint8_t ubX1, uint8_t ubY1, uint8_t ubColor, uint8_t ubUpdate);
void ssd1306_draw_rect(uint8_t ubX, uint8_t ubY, uint8_t ubWidth, uint8_t ubHeight, uint8_t ubFill, uint8_t ubColor, uint8_t ubUpdate);
void ssd1306_draw_circle(uint8_t ubX, uint8_t ubY, uint8_t ubRadius, uint8_t ubColor, uint8_t ubUpdate);
void ssd1306_draw_string(uint8_t ubX, uint8_t ubY, const char* pszString, uint8_t ubColor, uint8_t ubUpdate);

void ssd1306_set_scroll(uint8_t ubDirection, uint8_t ubStart, uint8_t ubStop);

void ssd1306_set_brightness(uint8_t ubBrightness);
void ssd1306_set_status(uint8_t ubStatus);
void ssd1306_set_inverted(uint8_t ubInverted);

void ssd1306_clear_display(uint8_t ubUpdate);
void ssd1306_update();
void ssd1306_ready_wait();

#endif // __SSD1306_H__