#include "display.h"
#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

#ifndef F_CPU
#define F_CPU 8000000UL
#endif

#define OLED_RST_PORT  PORTD
#define OLED_RST_DDR   DDRD
#define OLED_RST_PIN   PD6

// Match Adafruit SSD1327 definitions
#define SSD1327_SETCONTRAST      0x81
#define SSD1327_SETCOLUMN        0x15
#define SSD1327_SETROW           0x75
#define SSD1327_SEGREMAP         0xA0
#define SSD1327_SETSTARTLINE     0xA1
#define SSD1327_SETDISPLAYOFFSET 0xA2
#define SSD1327_NORMALDISPLAY    0xA4
#define SSD1327_DISPLAYALLON     0xA5
#define SSD1327_DISPLAYALLOFF    0xA6
#define SSD1327_SETMULTIPLEX     0xA8
#define SSD1327_REGULATOR        0xAB
#define SSD1327_DISPLAYOFF       0xAE
#define SSD1327_DISPLAYON        0xAF
#define SSD1327_PHASELEN         0xB1
#define SSD1327_DCLK             0xB3
#define SSD1327_PRECHARGE2       0xB6
#define SSD1327_PRECHARGE        0xBC
#define SSD1327_SETVCOM          0xBE
#define SSD1327_FUNCSELB         0xD5
#define SSD1327_CMDLOCK          0xFD

#define CTRL_CMD   0x80
#define CTRL_DATA  0x40

static uint8_t oled_buffer[OLED_BUFFER_SIZE];

static inline void oled_rst_low(void)  { OLED_RST_PORT &= ~(1 << OLED_RST_PIN); }
static inline void oled_rst_high(void) { OLED_RST_PORT |=  (1 << OLED_RST_PIN); }

static void oled_reset(void) {
    OLED_RST_DDR |= (1 << OLED_RST_PIN);
    oled_rst_high();
    _delay_ms(10);
    oled_rst_low();
    _delay_ms(20);
    oled_rst_high();
    _delay_ms(20);
}

static void i2c_init(void) {
    // ATmega1284P: PC0 = SCL, PC1 = SDA
    DDRC &= ~((1 << PC0) | (1 << PC1));
    PORTC |= (1 << PC0) | (1 << PC1);

    TWSR = 0x00; // prescaler = 1
    TWBR = (uint8_t)(((F_CPU / 100000UL) - 16) / 2);
    TWCR = (1 << TWEN);
}

static uint8_t i2c_start_write(uint8_t addr7) {
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT))) {}

    TWDR = (addr7 << 1) | 0;
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT))) {}

    return ((TWSR & 0xF8) == 0x18);
}

static uint8_t i2c_write(uint8_t data) {
    TWDR = data;
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT))) {}

    return ((TWSR & 0xF8) == 0x28);
}

static void i2c_stop(void) {
    TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
    while (TWCR & (1 << TWSTO)) {}
}

static uint8_t oled_command(uint8_t cmd) {
    if (!i2c_start_write(OLED_I2C_ADDR)) return 0;
    if (!i2c_write(CTRL_CMD)) { i2c_stop(); return 0; }
    if (!i2c_write(cmd))      { i2c_stop(); return 0; }
    i2c_stop();
    return 1;
}

static uint8_t oled_command_list(const uint8_t *cmds, uint8_t len) {
    if (!i2c_start_write(OLED_I2C_ADDR)) return 0;
    if (!i2c_write(0x00)) { i2c_stop(); return 0; } // Adafruit-style command stream
    for (uint8_t i = 0; i < len; i++) {
        if (!i2c_write(cmds[i])) {
            i2c_stop();
            return 0;
        }
    }
    i2c_stop();
    return 1;
}

static uint8_t oled_write_data_stream(const uint8_t *data, uint8_t len) {
    if (!i2c_start_write(OLED_I2C_ADDR)) return 0;
    if (!i2c_write(CTRL_DATA)) { i2c_stop(); return 0; }
    for (uint8_t i = 0; i < len; i++) {
        if (!i2c_write(data[i])) {
            i2c_stop();
            return 0;
        }
    }
    i2c_stop();
    return 1;
}

void oled_init(void) {
    i2c_init();
    oled_reset();

    // Closely follow Adafruit init for 128x128
    static const uint8_t init_128x128[] = {
        SSD1327_DISPLAYOFF,
        SSD1327_SETCONTRAST,      0x80,
        SSD1327_SEGREMAP,         0x51,
        SSD1327_SETSTARTLINE,     0x00,
        SSD1327_SETDISPLAYOFFSET, 0x00,
        SSD1327_DISPLAYALLOFF,
        SSD1327_SETMULTIPLEX,     0x7F,
        SSD1327_PHASELEN,         0x11,
        SSD1327_DCLK,             0x00,
        SSD1327_REGULATOR,        0x01,
        SSD1327_PRECHARGE2,       0x04,
        SSD1327_SETVCOM,          0x0F,
        SSD1327_PRECHARGE,        0x08,
        SSD1327_FUNCSELB,         0x62,
        SSD1327_CMDLOCK,          0x12,
        SSD1327_NORMALDISPLAY,
        SSD1327_DISPLAYON
    };

    oled_command_list(init_128x128, sizeof(init_128x128));
    _delay_ms(100);
    oled_command(SSD1327_DISPLAYON);
    oled_command(SSD1327_SETCONTRAST);
    oled_command(0x2F);

    oled_clear(0x0);
    oled_update();
}

void oled_clear(uint8_t shade) {
    shade &= 0x0F;
    uint8_t packed = (shade << 4) | shade;

    for (uint16_t i = 0; i < OLED_BUFFER_SIZE; i++) {
        oled_buffer[i] = packed;
    }
}

void oled_draw_pixel(uint8_t x, uint8_t y, uint8_t shade) {
    if (x >= OLED_WIDTH || y >= OLED_HEIGHT) return;

    shade &= 0x0F;
    uint16_t index = ((uint16_t)y * (OLED_WIDTH / 2)) + (x / 2);

    if ((x & 1) == 0) {
        oled_buffer[index] = (oled_buffer[index] & 0x0F) | (shade << 4);
    } else {
        oled_buffer[index] = (oled_buffer[index] & 0xF0) | shade;
    }
}

void oled_update(void) {
    uint8_t bytes_per_row = OLED_WIDTH / 2; // 64
    uint8_t maxbuff = 16; // conservative AVR-safe chunk

    // Adafruit sends row first, then column
    uint8_t cmd[] = {
        SSD1327_SETROW,    0x00, 0x7F,
        SSD1327_SETCOLUMN, 0x00, 0x3F
    };
    oled_command_list(cmd, sizeof(cmd));

    for (uint8_t row = 0; row < OLED_HEIGHT; row++) {
        uint8_t *ptr = &oled_buffer[(uint16_t)row * bytes_per_row];
        uint8_t bytes_remaining = bytes_per_row;

        while (bytes_remaining) {
            uint8_t to_write = (bytes_remaining > maxbuff) ? maxbuff : bytes_remaining;
            oled_write_data_stream(ptr, to_write);
            ptr += to_write;
            bytes_remaining -= to_write;
        }
    }
}

static const uint8_t font5x7[128][5] = {
    ['a'] = {0x20, 0x54, 0x54, 0x54, 0x78},
    ['b'] = {0x7F, 0x48, 0x44, 0x44, 0x38},
    ['c'] = {0x38, 0x44, 0x44, 0x44, 0x20},
    ['d'] = {0x38, 0x44, 0x44, 0x48, 0x7F},
    ['e'] = {0x38, 0x54, 0x54, 0x54, 0x18},
    ['f'] = {0x08, 0x7E, 0x09, 0x01, 0x02},
    ['g'] = {0x0C, 0x52, 0x52, 0x52, 0x3E},
    ['h'] = {0x7F, 0x08, 0x04, 0x04, 0x78},
    ['i'] = {0x00, 0x44, 0x7D, 0x40, 0x00},
    ['j'] = {0x20, 0x40, 0x44, 0x3D, 0x00},
    ['k'] = {0x7F, 0x10, 0x28, 0x44, 0x00},
    ['l'] = {0x00, 0x41, 0x7F, 0x40, 0x00},
    ['m'] = {0x7C, 0x04, 0x18, 0x04, 0x78},
    ['n'] = {0x7C, 0x08, 0x04, 0x04, 0x78},
    ['o'] = {0x38, 0x44, 0x44, 0x44, 0x38},
    ['p'] = {0x7C, 0x14, 0x14, 0x14, 0x08},
    ['q'] = {0x08, 0x14, 0x14, 0x18, 0x7C},
    ['r'] = {0x7C, 0x08, 0x04, 0x04, 0x08},
    ['s'] = {0x48, 0x54, 0x54, 0x54, 0x20},
    ['t'] = {0x04, 0x3F, 0x44, 0x40, 0x20},
    ['u'] = {0x3C, 0x40, 0x40, 0x20, 0x7C},
    ['v'] = {0x1C, 0x20, 0x40, 0x20, 0x1C},
    ['w'] = {0x3C, 0x40, 0x30, 0x40, 0x3C},
    ['x'] = {0x44, 0x28, 0x10, 0x28, 0x44},
    ['y'] = {0x0C, 0x50, 0x50, 0x50, 0x3C},
    ['z'] = {0x44, 0x64, 0x54, 0x4C, 0x44},

    ['0'] = {0x3E, 0x51, 0x49, 0x45, 0x3E},
    ['1'] = {0x00, 0x42, 0x7F, 0x40, 0x00},
    ['2'] = {0x42, 0x61, 0x51, 0x49, 0x46},
    ['3'] = {0x21, 0x41, 0x45, 0x4B, 0x31},
    ['4'] = {0x18, 0x14, 0x12, 0x7F, 0x10},
    ['5'] = {0x27, 0x45, 0x45, 0x45, 0x39},
    ['6'] = {0x3C, 0x4A, 0x49, 0x49, 0x30},
    ['7'] = {0x01, 0x71, 0x09, 0x05, 0x03},
    ['8'] = {0x36, 0x49, 0x49, 0x49, 0x36},
    ['9'] = {0x06, 0x49, 0x49, 0x29, 0x1E},

    [':'] = {0x00, 0x36, 0x36, 0x00, 0x00},
    ['.'] = {0x00, 0x60, 0x60, 0x00, 0x00},
    ['!'] = {0x00, 0x00, 0x5F, 0x00, 0x00},
    [' '] = {0x00, 0x00, 0x00, 0x00, 0x00},
    ['-'] = {0x08, 0x08, 0x08, 0x08, 0x08},
    ['>'] = {0x00, 0x41, 0x22, 0x14, 0x08}
};

void draw_char(uint8_t x, uint8_t y, char c, uint8_t scale, uint8_t color) {
    if ((uint8_t)c >= 128) return;

    for (uint8_t col = 0; col < 5; col++) {
        uint8_t line = font5x7[(uint8_t)c][col];

        for (uint8_t row = 0; row < 7; row++) {
            if (line & (1 << row)) {
                for (uint8_t dx = 0; dx < scale; dx++) {
                    for (uint8_t dy = 0; dy < scale; dy++) {
                        oled_draw_pixel(
                            x + col * scale + dx,
                            y + row * scale + dy,
                            color
                        );
                    }
                }
            }
        }
    }
}

void draw_string(uint8_t x, uint8_t y, const char *str, uint8_t scale, uint8_t color) {
    while (*str) {
        draw_char(x, y, *str, scale, color);
        x += 6 * scale; // 5 pixels + 1 spacing
        str++;
    }
}
