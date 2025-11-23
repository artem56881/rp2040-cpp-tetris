#ifndef _FONTS_H
#define _FONTS_H

#include <stdint.h>

typedef struct {
    const uint8_t width;    /* Font width in pixels */
    const uint8_t height;   /* Font height in pixels */
    const uint16_t *data;   /* Pointer to data font data array */
} FontDef;

extern FontDef Font_7x10;
extern FontDef Font_11x18;
extern FontDef Font_16x26;

#endif