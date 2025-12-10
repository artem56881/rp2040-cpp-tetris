# rp2040-cpp-tetris
tetris game for a rp2040 and a st7735 128x160 spi tft display st7735
* library used and modified:
https://gitverse.ru/gppsoft/ST7735_RP2040_Driver
modified to use a frameebuffer instead of sending data after each draw

## Building Guide

1.  **Clone the repository**

    ``` sh
    git clone https://github.com/artem56881/rp2040-cpp-tetris.git
    ```

2.  **Open in VSCode**\
    Import project into vscode using Raspberry Pi Pico vscode extention.

3.  **Compile the project**\
    Press **Compile** in the bottom-right.

4.  **Enter BOOTSEL mode**\
    Connect your **piqo™** to the PC while holding the BOOTSEL button.

5.  **Flash the firmware**\
    Copy:

        ./build/ssd1306_i2c.uf2

    into the newly appeared flash drive.

Or use pre-build uf2 if dont want to modify.
