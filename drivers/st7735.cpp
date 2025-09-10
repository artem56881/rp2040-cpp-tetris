#include "st7735.h"

// based on Adafruit ST7735 library for Arduino
static const uint8_t init_cmds1[] = { // Init for 7735R, part 1 (red or green tab)
    15,                               // 15 commands in list:
    ST7735_SWRESET, DELAY,            //  1: Software reset, 0 args, w/delay
    150,                              //     150 ms delay
    ST7735_SLPOUT, DELAY,             //  2: Out of sleep mode, 0 args, w/delay
    255,                              //     500 ms delay
    ST7735_FRMCTR1, 3,                //  3: Frame rate ctrl - normal mode, 3 args:
    0x01, 0x2C, 0x2D,
    ST7735_FRMCTR2, 3,
    0x01, 0x2C, 0x2D,
    ST7735_FRMCTR3, 6,
    0x01, 0x2C, 0x2D,
    0x01, 0x2C, 0x2D,
    ST7735_INVCTR, 1,
    0x07,
    ST7735_PWCTR1, 3,
    0xA2, 0x02,
    0x84,
    ST7735_PWCTR2, 1,
    0xC5,
    ST7735_PWCTR3, 2,
    0x0A, 0x00,
    ST7735_PWCTR4, 2,
    0x8A, 0x2A,
    ST7735_PWCTR5, 2,
    0x8A, 0xEE,
    ST7735_VMCTR1, 1,
    0x0E,
    ST7735_INVOFF, 0,
    ST7735_MADCTL, 1,
    ST7735_DATA_ROTATION,
    ST7735_COLMOD, 1,
    0x05},

    init_cmds2[] = { // Init for 7735R, part 2 (1.44" display)
        2,
        ST7735_CASET, 4,
        0x00, 0x00,
        0x00, 0x7F,
        ST7735_RASET, 4,
        0x00, 0x00,
        0x00, 0x7F},

    init_cmds3[] = { // Init for 7735R, part 3 (red or green tab)
        4,
        ST7735_GMCTRP1,
        16,
        0x02, 0x1c, 0x07, 0x12, 0x37, 0x32, 0x29, 0x2d, 0x29, 0x25, 0x2B, 0x39, 0x00, 0x01, 0x03, 0x10,
        ST7735_GMCTRN1,
        16,
        0x03, 0x1d, 0x07, 0x06, 0x2E, 0x2C, 0x29, 0x2D, 0x2E, 0x2E, 0x37, 0x3F, 0x00, 0x00, 0x02, 0x10,
        ST7735_NORON, DELAY,
        10,
        ST7735_DISPON, DELAY,
        100};

// physical panel size (constants)
#define FB_WIDTH  ST7735_WIDTH
#define FB_HEIGHT ST7735_HEIGHT

// framebuffer: store 16-bit color (RGB565) per pixel
static uint16_t framebuffer[FB_WIDTH * FB_HEIGHT];

// logical drawing area (depends on rotation)
static int16_t _width = ST7735_WIDTH, _height = ST7735_HEIGHT;
static uint8_t _xstart = ST7735_XSTART, _ystart = ST7735_YSTART;
static uint8_t _data_rotation[4] = {ST7735_MADCTL_MX, ST7735_MADCTL_MY, ST7735_MADCTL_MV, ST7735_MADCTL_BGR};
static uint8_t _rotation = 0; // 0..3

uint16_t rgb565_to_bgr565(uint16_t rgb565) {
    uint8_t r = (rgb565 >> 11) & 0x1F;
    uint8_t g = (rgb565 >> 5)  & 0x3F;
    uint8_t b = rgb565         & 0x1F;
    return ((uint16_t)b << 11) | ((uint16_t)g << 5) | (uint16_t)r;
}

// Инициализация SPI
static void ST7735_SPI_Init()
{
    spi_init(spi_default, 62500 * 1000); // 62.5 MHz (максимум для RP2040)

    gpio_set_function(PIN_LCD_DIN, GPIO_FUNC_SPI);
    gpio_set_function(PIN_LCD_CLK, GPIO_FUNC_SPI);

    // Инициализация управляющих пинов
    gpio_init(PIN_LCD_CS);
    gpio_init(PIN_LCD_DC);
    gpio_init(PIN_LCD_RST);
    gpio_init(PIN_LCD_BL);

    gpio_set_dir(PIN_LCD_CS, GPIO_OUT);
    gpio_set_dir(PIN_LCD_DC, GPIO_OUT);
    gpio_set_dir(PIN_LCD_RST, GPIO_OUT);
    gpio_set_dir(PIN_LCD_BL, GPIO_OUT);

    gpio_put(PIN_LCD_CS, 1);
    gpio_put(PIN_LCD_DC, 1);
    gpio_put(PIN_LCD_BL, 1); // Включить подсветку
}

static void ST7735_Reset()
{
    gpio_put(PIN_LCD_RST, 0);
    sleep_ms(100);
    gpio_put(PIN_LCD_RST, 1);
    sleep_ms(100);
}

// Отправка команды на дисплей
static void ST7735_WriteCommand(uint8_t cmd)
{
    gpio_put(PIN_LCD_CS, 0); // Активировать чип
    gpio_put(PIN_LCD_DC, 0); // Командный режим
    spi_write_blocking(spi_default, &cmd, 1);
    gpio_put(PIN_LCD_CS, 1); // Деактивировать чип
}

// Отправка данных на дисплей
static void ST7735_WriteData(uint8_t *data, size_t buff_size)
{
    gpio_put(PIN_LCD_CS, 0); // Активировать чип
    gpio_put(PIN_LCD_DC, 1); // Режим данных
    spi_write_blocking(spi_default, data, buff_size);
    gpio_put(PIN_LCD_CS, 1); // Деактивировать чип
}

static void ST7735_SetAddressWindow(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1)
{
    // column address set
    ST7735_WriteCommand(ST7735_CASET);
    uint8_t dataCol[] = {0x00, (uint8_t)(x0 + _xstart), 0x00, (uint8_t)(x1 + _xstart)};
    ST7735_WriteData(dataCol, sizeof(dataCol));

    // row address set
    ST7735_WriteCommand(ST7735_RASET);
    uint8_t dataRow[] = {0x00, (uint8_t)(y0 + _ystart), 0x00, (uint8_t)(y1 + _ystart)};
    ST7735_WriteData(dataRow, sizeof(dataRow));

    // write to RAM
    ST7735_WriteCommand(ST7735_RAMWR);
}

static void ST7735_ExecuteCommandList(const uint8_t *addr)
{
    uint8_t numCommands, numArgs;
    uint16_t ms;

    numCommands = *addr++;
    while (numCommands--)
    {
        uint8_t cmd = *addr++;

        ST7735_WriteCommand(cmd);

        numArgs = *addr++;
        // If high bit set, delay follows args
        ms = numArgs & DELAY;
        numArgs &= ~DELAY;

        if (numArgs)
        {
            ST7735_WriteData((uint8_t *)addr, numArgs);
            addr += numArgs;
        }

        if (ms)
        {
            ms = *addr++;
            if (ms == 255)
                ms = 500;
            sleep_ms(ms);
        }
    }
}

// ---------- Framebuffer helpers ----------

// Map logical (x,y) (0.._width-1, 0.._height-1) to physical framebuffer coords (px,py)
static inline void map_logical_to_physical(int16_t x, int16_t y, int16_t *px, int16_t *py)
{
    switch (_rotation & 3)
    {
    case 0:
        *px = x;
        *py = y;
        break;
    case 1:
        *px = y;
        *py = (FB_HEIGHT - 1) - x;
        break;
    case 2:
        *px = (FB_WIDTH - 1) - x;
        *py = (FB_HEIGHT - 1) - y;
        break;
    case 3:
        *px = (FB_WIDTH - 1) - y;
        *py = x;
        break;
    default:
        *px = x;
        *py = y;
    }
}

// Set pixel into framebuffer (no bounds check of physical buffer needed if mapping correct)
static inline void fb_set_pixel(int16_t x, int16_t y, uint16_t color)
{
    // x,y are logical coordinates (0.._width-1 / 0.._height-1)
    if ((x < 0) || (y < 0) || (x >= _width) || (y >= _height))
        return;
    int16_t px, py;
    map_logical_to_physical(x, y, &px, &py);
    if ((px < 0) || (py < 0) || (px >= FB_WIDTH) || (py >= FB_HEIGHT))
        return;
    framebuffer[py * FB_WIDTH + px] = rgb565_to_bgr565(color);
}

// Get pixel from framebuffer (logical coords)
static inline uint16_t fb_get_pixel(int16_t x, int16_t y)
{
    if ((x < 0) || (y < 0) || (x >= _width) || (y >= _height))
        return 0;
    int16_t px, py;
    map_logical_to_physical(x, y, &px, &py);
    if ((px < 0) || (py < 0) || (px >= FB_WIDTH) || (py >= FB_HEIGHT))
        return 0;
    return framebuffer[py * FB_WIDTH + px];
}

// Fill entire framebuffer with color
void ST7735_FillBuffer(uint16_t color)
{
    for (uint32_t i = 0; i < (uint32_t)FB_WIDTH * (uint32_t)FB_HEIGHT; ++i)
        framebuffer[i] = color;
}

// ---------- Public API (drawing now writes only to framebuffer) ----------

void ST7735_Init(void)
{
    ST7735_SPI_Init();

    gpio_put(PIN_LCD_CS, 0);

    ST7735_Reset();

    ST7735_ExecuteCommandList(init_cmds1);
    ST7735_ExecuteCommandList(init_cmds2);
    ST7735_ExecuteCommandList(init_cmds3);

    gpio_put(PIN_LCD_CS, 1);

    // initialize framebuffer to black
    ST7735_FillBuffer(0x0000);
    // default rotation already set by constants; set _width/_height accordingly
    _rotation = 0;
    _width = ST7735_WIDTH;
    _height = ST7735_HEIGHT;
}

// NOTE: drawing functions now only modify framebuffer

void ST7735_DrawPixel(uint16_t x, uint16_t y, uint16_t color)
{
    fb_set_pixel(x, y, color);
}

void ST7735_FillScreen(uint16_t color)
{
    // logical fill: fill only logical area (_width x _height)
    for (int16_t y = 0; y < _height; ++y)
        for (int16_t x = 0; x < _width; ++x)
            fb_set_pixel(x, y, color);
}

void ST7735_DrawChar(uint16_t x, uint16_t y, char ch, FontDef font, uint16_t color)
{
    uint32_t i, b, j;
    for (i = 0; i < font.height; i++)
    {
        b = font.data[(ch - 32) * font.height + i];
        for (j = 0; j < font.width; j++)
        {
            if ((b << j) & 0x8000)
            {
                ST7735_DrawPixel(x + j, y + i, color);
            }
        }
    }
}

void ST7735_DrawString(uint16_t x, uint16_t y, const char *str, FontDef font, uint16_t color)
{
    while (*str)
    {
        if (x + font.width >= _width)
        {
            x = 0;
            y += font.height;
            if (y + font.height >= _height)
            {
                break;
            }

            if (*str == ' ')
            {
                // skip spaces in the beginning of the new line
                str++;
                continue;
            }
        }

        ST7735_DrawChar(x, y, *str, font, color);
        x += font.width;
        str++;
    }
}

void ST7735_DrawRectFill(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    if ((x >= _width) || (y >= _height))
        return;
    if ((x + w - 1) >= _width)
        w = _width - x;
    if ((y + h - 1) >= _height)
        h = _height - y;

    for (uint16_t yy = 0; yy < h; yy++)
        for (uint16_t xx = 0; xx < w; xx++)
            fb_set_pixel(x + xx, y + yy, color);
}

void ST7735_DrawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    ST7735_DrawFastHLine(x, y, w, color);
    ST7735_DrawFastHLine(x, y + h - 1, w, color);
    ST7735_DrawFastVLine(x, y, h, color);
    ST7735_DrawFastVLine(x + w - 1, y, h, color);
}

void ST7735_DrawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color)
{
    if ((x >= _width) || (y >= _height))
        return;
    if ((y + h - 1) >= _height)
        h = _height - y;

    for (int16_t i = 0; i < h; ++i)
        fb_set_pixel(x, y + i, color);
}

void ST7735_DrawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color)
{
    if ((x >= _width) || (y >= _height))
        return;
    if ((x + w - 1) >= _width)
        w = _width - x;

    for (int16_t i = 0; i < w; ++i)
        fb_set_pixel(x + i, y, color);
}

void ST7735_DrawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color)
{
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -r - r;
    int16_t x = 0;

    ST7735_DrawPixel(x0 + r, y0, color);
    ST7735_DrawPixel(x0 - r, y0, color);
    ST7735_DrawPixel(x0, y0 - r, color);
    ST7735_DrawPixel(x0, y0 + r, color);

    while (x < r)
    {
        if (f >= 0)
        {
            r--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        ST7735_DrawPixel(x0 + x, y0 + r, color);
        ST7735_DrawPixel(x0 - x, y0 + r, color);
        ST7735_DrawPixel(x0 - x, y0 - r, color);
        ST7735_DrawPixel(x0 + x, y0 - r, color);

        ST7735_DrawPixel(x0 + r, y0 + x, color);
        ST7735_DrawPixel(x0 - r, y0 + x, color);
        ST7735_DrawPixel(x0 - r, y0 - x, color);
        ST7735_DrawPixel(x0 + r, y0 - x, color);
    }
}

static void ST7735_DrawCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t cornername, uint16_t color)
{
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;

    while (x < r)
    {
        if (f >= 0)
        {
            r--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;
        if (cornername & 0x8)
        {
            ST7735_DrawPixel(x0 - r, y0 + x, color);
            ST7735_DrawPixel(x0 - x, y0 + r, color);
        }
        if (cornername & 0x4)
        {
            ST7735_DrawPixel(x0 + x, y0 + r, color);
            ST7735_DrawPixel(x0 + r, y0 + x, color);
        }
        if (cornername & 0x2)
        {
            ST7735_DrawPixel(x0 + r, y0 - x, color);
            ST7735_DrawPixel(x0 + x, y0 - r, color);
        }
        if (cornername & 0x1)
        {
            ST7735_DrawPixel(x0 - x, y0 - r, color);
            ST7735_DrawPixel(x0 - r, y0 - x, color);
        }
    }
}

static void ST7735_FillCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t cornername, int16_t delta, uint16_t color)
{
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -r - r;
    int16_t x = 0;

    delta++;
    while (x < r)
    {
        if (f >= 0)
        {
            r--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        if (cornername & 0x1)
        {
            ST7735_DrawFastVLine(x0 + x, y0 - r, r + r + delta, color);
            ST7735_DrawFastVLine(x0 + r, y0 - x, x + x + delta, color);
        }
        if (cornername & 0x2)
        {
            ST7735_DrawFastVLine(x0 - x, y0 - r, r + r + delta, color);
            ST7735_DrawFastVLine(x0 - r, y0 - x, x + x + delta, color);
        }
    }
}

void ST7735_DrawCircleFill(int16_t x0, int16_t y0, int16_t r, uint16_t color)
{
    ST7735_DrawFastVLine(x0, y0 - r, r + r + 1, color);
    ST7735_FillCircleHelper(x0, y0, r, 3, 0, color);
}

void ST7735_DrawRectRound(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color)
{
    ST7735_DrawFastHLine(x + r, y, w - r - r, color);         // Top
    ST7735_DrawFastHLine(x + r, y + h - 1, w - r - r, color); // Bottom
    ST7735_DrawFastVLine(x, y + r, h - r - r, color);         // Left
    ST7735_DrawFastVLine(x + w - 1, y + r, h - r - r, color); // Right
    // draw four corners
    ST7735_DrawCircleHelper(x + r, y + r, r, 1, color);
    ST7735_DrawCircleHelper(x + r, y + h - r - 1, r, 8, color);
    ST7735_DrawCircleHelper(x + w - r - 1, y + r, r, 2, color);
    ST7735_DrawCircleHelper(x + w - r - 1, y + h - r - 1, r, 4, color);
}

void ST7735_DrawRectRoundFill(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color)
{
    ST7735_DrawRectFill(x + r, y, w - r - r, h, color);

    // draw four corners
    ST7735_FillCircleHelper(x + w - r - 1, y + r, r, 1, h - r - r - 1, color);
    ST7735_FillCircleHelper(x + r, y + r, r, 2, h - r - r - 1, color);
}

void ST7735_DrawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color)
{
    ST7735_DrawLine(x0, y0, x1, y1, color);
    ST7735_DrawLine(x1, y1, x2, y2, color);
    ST7735_DrawLine(x2, y2, x0, y0, color);
}

void ST7735_DrawTriangleFill(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color)
{
    int16_t a, b, y, last;

    // Sort coordinates by Y order (y2 >= y1 >= y0)
    if (y0 > y1)
    {
        SWAP_INT16_T(y0, y1);
        SWAP_INT16_T(x0, x1);
    }

    if (y1 > y2)
    {
        SWAP_INT16_T(y2, y1);
        SWAP_INT16_T(x2, x1);
    }

    if (y0 > y1)
    {
        SWAP_INT16_T(y0, y1);
        SWAP_INT16_T(x0, x1);
    }

    if (y0 == y2)
    { // Handle awkward all-on-same-line case as its own thing
        a = b = x0;
        if (x1 < a)
            a = x1;
        else if (x1 > b)
            b = x1;
        if (x2 < a)
            a = x2;
        else if (x2 > b)
            b = x2;
        ST7735_DrawFastHLine(a, y0, b - a + 1, color);
        return;
    }

    int16_t dx01 = x1 - x0, dy01 = y1 - y0, dx02 = x2 - x0, dy02 = y2 - y0,
            dx12 = x2 - x1, dy12 = y2 - y1, sa = 0, sb = 0;

    if (y1 == y2)
        last = y1; // Include y1 scanline
    else
        last = y1 - 1; // Skip it

    for (y = y0; y <= last; y++)
    {
        a = x0 + sa / dy01;
        b = x0 + sb / dy02;
        sa += dx01;
        sb += dx02;

        if (a > b)
            SWAP_INT16_T(a, b);
        ST7735_DrawFastHLine(a, y, b - a + 1, color);
    }

    sa = dx12 * (y - y1);
    sb = dx02 * (y - y0);
    for (; y <= y2; y++)
    {
        a = x1 + sa / dy12;
        b = x0 + sb / dy02;
        sa += dx12;
        sb += dx02;

        if (a > b)
            SWAP_INT16_T(a, b);
        ST7735_DrawFastHLine(a, y, b - a + 1, color);
    }
}

#ifdef FAST_LINE
void ST7735_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
{

    uint8_t data[] = {color >> 8, color & 0xFF};

    int16_t steep = abs(y1 - y0) > abs(x1 - x0);
    if (steep)
    {
        SWAP_INT16_T(x0, y0);
        SWAP_INT16_T(x1, y1);
    }

    if (x0 > x1)
    {
        SWAP_INT16_T(x0, x1);
        SWAP_INT16_T(y0, y1);
    }

    if (x1 < 0)
        return;

    int16_t dx, dy;
    dx = x1 - x0;
    dy = abs(y1 - y0);

    int16_t err = dx / 2;
    int8_t ystep = (y0 < y1) ? 1 : (-1);

    if (steep) // y increments every iteration (y0 is x-axis, and x0 is y-axis)
    {
        if (x1 >= _height)
            x1 = _height - 1;

        for (; x0 <= x1; x0++)
        {
            if ((x0 >= 0) && (y0 >= 0) && (y0 < _width))
                break;
            err -= dy;
            if (err < 0)
            {
                err += dx;
                y0 += ystep;
            }
        }

        if (x0 > x1)
            return;

        for (; x0 <= x1; x0++)
        {
            ST7735_DrawPixel(y0, x0, color);

            err -= dy;
            if (err < 0)
            {
                y0 += ystep;
                if ((y0 < 0) || (y0 >= _width))
                    break;
                err += dx;
            }
        }
    }
    else // x increments every iteration (x0 is x-axis, and y0 is y-axis)
    {
        if (x1 >= _width)
            x1 = _width - 1;

        for (; x0 <= x1; x0++)
        {
            if ((x0 >= 0) && (y0 >= 0) && (y0 < _height))
                break;
            err -= dy;
            if (err < 0)
            {
                err += dx;
                y0 += ystep;
            }
        }

        if (x0 > x1)
            return;

        for (; x0 <= x1; x0++)
        {
            ST7735_DrawPixel(x0, y0, color);

            err -= dy;
            if (err < 0)
            {
                y0 += ystep;
                if ((y0 < 0) || (y0 >= _height))
                    break;
                err += dx;
            }
        }
    }
}
#else
void ST7735_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
{

    int16_t steep = abs(y1 - y0) > abs(x1 - x0);
    if (steep)
    {
        SWAP_INT16_T(x0, y0);
        SWAP_INT16_T(x1, y1);
    }

    if (x0 > x1)
    {
        SWAP_INT16_T(x0, x1);
        SWAP_INT16_T(y0, y1);
    }

    int16_t dx, dy;
    dx = x1 - x0;
    dy = abs(y1 - y0);

    int16_t err = dx / 2;
    int16_t ystep;

    if (y0 < y1)
    {
        ystep = 1;
    }
    else
    {
        ystep = -1;
    }

    for (; x0 <= x1; x0++)
    {
        if (steep)
        {
            ST7735_DrawPixel(y0, x0, color);
        }
        else
        {
            ST7735_DrawPixel(x0, y0, color);
        }
        err -= dy;
        if (err < 0)
        {
            y0 += ystep;
            err += dx;
        }
    }
}
#endif

void ST7735_SetRotation(uint8_t rotation)
{
    _rotation = rotation & 3;
    ST7735_WriteCommand(ST7735_MADCTL);

    switch (_rotation)
    {
    case 0:
    {
        uint8_t d_r = (_data_rotation[0] | _data_rotation[1] | _data_rotation[3]);
        ST7735_WriteData(&d_r, sizeof(d_r));
        _width = ST7735_WIDTH;
        _height = ST7735_HEIGHT;
        _xstart = ST7735_XSTART;
        _ystart = ST7735_YSTART;
    }
    break;
    case 1:
    {
        uint8_t d_r = (_data_rotation[1] | _data_rotation[2] | _data_rotation[3]);
        ST7735_WriteData(&d_r, sizeof(d_r));
        _width = ST7735_HEIGHT;
        _height = ST7735_WIDTH;
        _xstart = ST7735_YSTART;
        _ystart = ST7735_XSTART;
    }
    break;
    case 2:
    {
        uint8_t d_r = _data_rotation[3];
        ST7735_WriteData(&d_r, sizeof(d_r));
        _width = ST7735_WIDTH;
        _height = ST7735_HEIGHT;
        _xstart = ST7735_XSTART;
        _ystart = ST7735_YSTART;
    }
    break;
    case 3:
    {
        uint8_t d_r = (_data_rotation[0] | _data_rotation[2] | _data_rotation[3]);
        ST7735_WriteData(&d_r, sizeof(d_r));
        _width = ST7735_HEIGHT;
        _height = ST7735_WIDTH;
        _xstart = ST7735_YSTART;
        _ystart = ST7735_XSTART;
    }
    break;
    }
}

// Draw image from array of uint16_t (RGB565). Source data assumed MSB-first per 16-bit value
void ST7735_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *data)
{
    if ((x >= _width) || (y >= _height))
        return;
    if ((x + w - 1) >= _width)
        return;
    if ((y + h - 1) >= _height)
        return;

    for (uint16_t yy = 0; yy < h; ++yy)
    {
        for (uint16_t xx = 0; xx < w; ++xx)
        {
            // data is an array of uint16_t in native host order; keep as-is
            uint16_t color = data[yy * w + xx];
            ST7735_DrawPixel(x + xx, y + yy, color);
        }
    }
}

void ST7735_BacklightOn()
{
    gpio_put(PIN_LCD_BL, 1);
}

void ST7735_BacklightOff()
{
    gpio_put(PIN_LCD_BL, 0);
}

void ST7735_InvertColors(bool invert)
{
    ST7735_WriteCommand(invert ? ST7735_INVON : ST7735_INVOFF);
}

// ---------- Buffer -> Display: call this to actually send framebuffer to panel ----------

void ST7735_Update()
{
    // Set the address window to cover the full physical panel
    ST7735_SetAddressWindow(0, 0, FB_WIDTH - 1, FB_HEIGHT - 1);

    // We'll send one row at a time as bytes MSB-first per pixel
    // temporary row buffer (FB_WIDTH * 2 bytes)
    uint8_t rowbuf[FB_WIDTH * 2];

    gpio_put(PIN_LCD_CS, 0);
    gpio_put(PIN_LCD_DC, 1);

    for (int py = 0; py < FB_HEIGHT; ++py)
    {
        // build row (MSB first per pixel)
        for (int px = 0; px < FB_WIDTH; ++px)
        {
            uint16_t c = framebuffer[py * FB_WIDTH + px];
            rowbuf[px * 2 + 0] = (uint8_t)(c >> 8);   // MSB
            rowbuf[px * 2 + 1] = (uint8_t)(c & 0xFF); // LSB
        }
        spi_write_blocking(spi_default, rowbuf, FB_WIDTH * 2);
    }

    gpio_put(PIN_LCD_CS, 1);
}

// ---------- helpers: clear logical area ----------

void ST7735_Clear()
{
    ST7735_FillScreen(0x0000);
}

// End of file
