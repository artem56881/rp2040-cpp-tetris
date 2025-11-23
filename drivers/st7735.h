#ifndef ST7735_H_
#define ST7735_H_

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "fonts.h"

#define PIN_LCD_DIN    3   // MOSI (SPI TX)
#define PIN_LCD_CLK    2   // SCK (SPI CLK)
#define PIN_LCD_CS     5   // Chip Select
#define PIN_LCD_DC     7   // Data/Command
#define PIN_LCD_RST    6   // Reset
#define PIN_LCD_BL     4   // Backlight (optional)

#define ST7735_MADCTL_MY  0x80
#define ST7735_MADCTL_MX  0x40
#define ST7735_MADCTL_MV  0x20
#define ST7735_MADCTL_RGB 0x00
#define ST7735_MADCTL_BGR 0x08

#define ST7735_WIDTH  			128
#define ST7735_HEIGHT 			160
#define ST7735_XSTART 			0
#define ST7735_YSTART 			0

#define ST7735_DATA_ROTATION (ST7735_MADCTL_MX | ST7735_MADCTL_MY | ST7735_MADCTL_BGR)

#define ST7735_NOP     0x00
#define ST7735_SWRESET 0x01
#define ST7735_RDDID   0x04
#define ST7735_RDDST   0x09

#define ST7735_SLPIN   0x10
#define ST7735_SLPOUT  0x11
#define ST7735_PTLON   0x12
#define ST7735_NORON   0x13

#define ST7735_INVOFF  0x20
#define ST7735_INVON   0x21
#define ST7735_DISPOFF 0x28
#define ST7735_DISPON  0x29
#define ST7735_CASET   0x2A
#define ST7735_RASET   0x2B
#define ST7735_RAMWR   0x2C
#define ST7735_RAMRD   0x2E

#define ST7735_PTLAR   0x30
#define ST7735_COLMOD  0x3A
#define ST7735_MADCTL  0x36

#define ST7735_FRMCTR1 0xB1
#define ST7735_FRMCTR2 0xB2
#define ST7735_FRMCTR3 0xB3
#define ST7735_INVCTR  0xB4
#define ST7735_DISSET5 0xB6

#define ST7735_PWCTR1  0xC0
#define ST7735_PWCTR2  0xC1
#define ST7735_PWCTR3  0xC2
#define ST7735_PWCTR4  0xC3
#define ST7735_PWCTR5  0xC4
#define ST7735_VMCTR1  0xC5

#define ST7735_RDID1   0xDA
#define ST7735_RDID2   0xDB
#define ST7735_RDID3   0xDC
#define ST7735_RDID4   0xDD

#define ST7735_PWCTR6  0xFC

#define ST7735_GMCTRP1 0xE0
#define ST7735_GMCTRN1 0xE1

// Color definitions
#define	ST7735_BLACK   0x0000
#define	ST7735_BLUE    0x001F
#define	ST7735_RED     0xF800
#define	ST7735_GREEN   0x07E0
#define ST7735_CYAN    0x07FF
#define ST7735_MAGENTA 0xF81F
#define ST7735_YELLOW  0xFFE0
#define ST7735_WHITE   0xFFFF

#define RGB_TO_RGB565(r, g, b) ((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))

#define FAST_LINE
#define DELAY 0x80

#define SWAP_INT16_T(a, b) { int16_t t = a; a = b; b = t; }
#define min(a, b) (((a) < (b)) ? (a) : (b))

void ST7735_Init(void);
void ST7735_FillScreen(uint16_t color);
void ST7735_DrawPixel(uint16_t x, uint16_t y, uint16_t color);
void ST7735_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *data);
void ST7735_DrawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color); 
void ST7735_DrawRectFill(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void ST7735_DrawRectRound(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r,uint16_t color);
void ST7735_DrawRectRoundFill(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r,uint16_t color);
void ST7735_DrawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color);
void ST7735_DrawCircleFill(int16_t x0, int16_t y0, int16_t r, uint16_t color);
void ST7735_DrawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color);
void ST7735_DrawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color);
void ST7735_DrawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,int16_t x2, int16_t y2, uint16_t color);
void ST7735_DrawTriangleFill(int16_t x0, int16_t y0, int16_t x1, int16_t y1,int16_t x2, int16_t y2, uint16_t color);
void ST7735_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);
void ST7735_DrawChar(uint16_t x, uint16_t y, char ch, FontDef font, uint16_t color);
void ST7735_DrawString(uint16_t x, uint16_t y, const char *str, FontDef font,uint16_t color);
void ST7735_BacklightOn();
void ST7735_BacklightOff();
void ST7735_InvertColors(bool invert);
void ST7735_SetRotation(uint8_t rotation);
void ST7735_Update();

#endif