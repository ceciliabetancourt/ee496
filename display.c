#include "display.h"
#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

// reset pin 
#define OLED_RST_PORT  PORTD
#define OLED_RST_DDR   DDRD
#define OLED_RST_PIN   PD6

// SSD1327 commands pulled from GitHub
// https://github.com/adafruit/Adafruit_SSD1327/blob/master/Adafruit_SSD1327.h
#define SSD1327_SETCONTRAST      0x81
#define SSD1327_NORMALDISPLAY    0xA6
#define SSD1327_DISPLAYOFF       0xAE
#define SSD1327_DISPLAYON        0xAF
#define SSD1327_SETCOLUMN        0x15
#define SSD1327_SETROW           0x75
#define SSD1327_SETREMAP         0xA0
#define SSD1327_SETSTARTLINE     0xA1
#define SSD1327_SETDISPLAYOFFSET 0xA2
#define SSD1327_DISPLAYALLOFF    0xA4
#define SSD1327_SETMULTIPLEX     0xA8
#define SSD1327_REGULATOR        0xAB
#define SSD1327_PHASELEN         0xB1
#define SSD1327_DCLK             0xB3
#define SSD1327_PRECHARGE2       0xB6
#define SSD1327_PRECHARGE        0xBC
#define SSD1327_SETVCOM          0xBE
#define SSD1327_CMDLOCK          0xFD
#define SSD1327_FUNCSELB         0xD5

// font for writing text: (ascii values from 32 to 126)
static const uint8_t font5x7[95][5] = {
    {0x00,0x00,0x00,0x00,0x00}, // ' '
    {0x00,0x00,0x5F,0x00,0x00}, // '!'
    {0x00,0x07,0x00,0x07,0x00}, // '"'
    {0x14,0x7F,0x14,0x7F,0x14}, // '#'
    {0x24,0x2A,0x7F,0x2A,0x12}, // '$'
    {0x23,0x13,0x08,0x64,0x62}, // '%'
    {0x36,0x49,0x55,0x22,0x50}, // '&'
    {0x00,0x05,0x03,0x00,0x00}, // '''
    {0x00,0x1C,0x22,0x41,0x00}, // '('
    {0x00,0x41,0x22,0x1C,0x00}, // ')'
    {0x14,0x08,0x3E,0x08,0x14}, // '*'
    {0x08,0x08,0x3E,0x08,0x08}, // '+'
    {0x00,0x50,0x30,0x00,0x00}, // ','
    {0x08,0x08,0x08,0x08,0x08}, // '-'
    {0x00,0x60,0x60,0x00,0x00}, // '.'
    {0x20,0x10,0x08,0x04,0x02}, // '/'
    {0x3E,0x51,0x49,0x45,0x3E}, // '0'
    {0x00,0x42,0x7F,0x40,0x00}, // '1'
    {0x42,0x61,0x51,0x49,0x46}, // '2'
    {0x21,0x41,0x45,0x4B,0x31}, // '3'
    {0x18,0x14,0x12,0x7F,0x10}, // '4'
    {0x27,0x45,0x45,0x45,0x39}, // '5'
    {0x3C,0x4A,0x49,0x49,0x30}, // '6'
    {0x01,0x71,0x09,0x05,0x03}, // '7'
    {0x36,0x49,0x49,0x49,0x36}, // '8'
    {0x06,0x49,0x49,0x29,0x1E}, // '9'
    {0x00,0x36,0x36,0x00,0x00}, // ':'
    {0x00,0x56,0x36,0x00,0x00}, // ';'
    {0x08,0x14,0x22,0x41,0x00}, // '<'
    {0x14,0x14,0x14,0x14,0x14}, // '='
    {0x00,0x41,0x22,0x14,0x08}, // '>'
    {0x02,0x01,0x51,0x09,0x06}, // '?'
    {0x32,0x49,0x79,0x41,0x3E}, // '@'
    {0x7E,0x11,0x11,0x11,0x7E}, // 'A'
    {0x7F,0x49,0x49,0x49,0x36}, // 'B'
    {0x3E,0x41,0x41,0x41,0x22}, // 'C'
    {0x7F,0x41,0x41,0x22,0x1C}, // 'D'
    {0x7F,0x49,0x49,0x49,0x41}, // 'E'
    {0x7F,0x09,0x09,0x09,0x01}, // 'F'
    {0x3E,0x41,0x49,0x49,0x7A}, // 'G'
    {0x7F,0x08,0x08,0x08,0x7F}, // 'H'
    {0x00,0x41,0x7F,0x41,0x00}, // 'I'
    {0x20,0x40,0x41,0x3F,0x01}, // 'J'
    {0x7F,0x08,0x14,0x22,0x41}, // 'K'
    {0x7F,0x40,0x40,0x40,0x40}, // 'L'
    {0x7F,0x02,0x0C,0x02,0x7F}, // 'M'
    {0x7F,0x04,0x08,0x10,0x7F}, // 'N'
    {0x3E,0x41,0x41,0x41,0x3E}, // 'O'
    {0x7F,0x09,0x09,0x09,0x06}, // 'P'
    {0x3E,0x41,0x51,0x21,0x5E}, // 'Q'
    {0x7F,0x09,0x19,0x29,0x46}, // 'R'
    {0x46,0x49,0x49,0x49,0x31}, // 'S'
    {0x01,0x01,0x7F,0x01,0x01}, // 'T'
    {0x3F,0x40,0x40,0x40,0x3F}, // 'U'
    {0x1F,0x20,0x40,0x20,0x1F}, // 'V'
    {0x3F,0x40,0x38,0x40,0x3F}, // 'W'
    {0x63,0x14,0x08,0x14,0x63}, // 'X'
    {0x07,0x08,0x70,0x08,0x07}, // 'Y'
    {0x61,0x51,0x49,0x45,0x43}, // 'Z'
    {0x00,0x7F,0x41,0x41,0x00}, // '['
    {0x02,0x04,0x08,0x10,0x20}, // '\'
    {0x00,0x41,0x41,0x7F,0x00}, // ']'
    {0x04,0x02,0x01,0x02,0x04}, // '^'
    {0x40,0x40,0x40,0x40,0x40}, // '_'
    {0x00,0x01,0x02,0x04,0x00}, // '`'
    {0x20,0x54,0x54,0x54,0x78}, // 'a'
    {0x7F,0x48,0x44,0x44,0x38}, // 'b'
    {0x38,0x44,0x44,0x44,0x20}, // 'c'
    {0x38,0x44,0x44,0x48,0x7F}, // 'd'
    {0x38,0x54,0x54,0x54,0x18}, // 'e'
    {0x08,0x7E,0x09,0x01,0x02}, // 'f'
    {0x0C,0x52,0x52,0x52,0x3E}, // 'g'
    {0x7F,0x08,0x04,0x04,0x78}, // 'h'
    {0x00,0x44,0x7D,0x40,0x00}, // 'i'
    {0x20,0x40,0x44,0x3D,0x00}, // 'j'
    {0x7F,0x10,0x28,0x44,0x00}, // 'k'
    {0x00,0x41,0x7F,0x40,0x00}, // 'l'
    {0x7C,0x04,0x18,0x04,0x78}, // 'm'
    {0x7C,0x08,0x04,0x04,0x78}, // 'n'
    {0x38,0x44,0x44,0x44,0x38}, // 'o'
    {0x7C,0x14,0x14,0x14,0x08}, // 'p'
    {0x08,0x14,0x14,0x18,0x7C}, // 'q'
    {0x7C,0x08,0x04,0x04,0x08}, // 'r'
    {0x48,0x54,0x54,0x54,0x20}, // 's'
    {0x04,0x3F,0x44,0x40,0x20}, // 't'
    {0x3C,0x40,0x40,0x20,0x7C}, // 'u'
    {0x1C,0x20,0x40,0x20,0x1C}, // 'v'
    {0x3C,0x40,0x30,0x40,0x3C}, // 'w'
    {0x44,0x28,0x10,0x28,0x44}, // 'x'
    {0x0C,0x50,0x50,0x50,0x3C}, // 'y'
    {0x44,0x64,0x54,0x4C,0x44}, // 'z'
    {0x00,0x08,0x36,0x41,0x00}, // '{'
    {0x00,0x00,0x7F,0x00,0x00}, // '|'
    {0x00,0x41,0x36,0x08,0x00}, // '}'
    {0x08,0x04,0x08,0x10,0x08}  // '~'
};

// I2C control bytes
#define SSD1327_CONTROL_CMD  0x80
#define SSD1327_CONTROL_DATA 0x40

static uint8_t oled_buffer[OLED_BUFFER_SIZE];

// reset helpers
static inline void oled_rst_low(void)  { OLED_RST_PORT &= ~(1 << OLED_RST_PIN); }
static inline void oled_rst_high(void) { OLED_RST_PORT |=  (1 << OLED_RST_PIN); }

static void oled_reset(void) {
    oled_rst_low();
    _delay_ms(20);
    oled_rst_high();
    _delay_ms(20);
}

// I2C low-level
static void i2c_init(void) {
    // PC1 = SDA, PC0 = SCL on ATmega1284P
    DDRC &= ~((1 << PC1) | (1 << PC0));   // inputs, TWI hardware controls them
    PORTC |= (1 << PC1) | (1 << PC0);     // optional weak pull-ups

    TWSR = 0x00;                          // prescaler = 1

    // 100 kHz: TWBR = ((F_CPU / SCL) - 16) / 2
    // For F_CPU = 16MHz: TWBR = 72
    TWBR = 72;

    TWCR = (1 << TWEN);
}

static uint8_t i2c_start(uint8_t address_write) {
    TWCR = (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT))) {}

    TWDR = address_write;
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT))) {}

    uint8_t status = TWSR & 0xF8;
    return (status == 0x18);  // SLA+W transmitted, ACK received
}

static void i2c_stop(void) {
    TWCR = (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
    while (TWCR & (1 << TWSTO)) {}
}

static uint8_t i2c_write(uint8_t data) {
    TWDR = data;
    TWCR = (1 << TWINT) | (1 << TWEN);
    while (!(TWCR & (1 << TWINT))) {}

    uint8_t status = TWSR & 0xF8;
    return (status == 0x28);  // data transmitted, ACK received
}

// SSD1327 over I2C
static uint8_t oled_write_command(uint8_t cmd) {
    if (!i2c_start((OLED_I2C_ADDR << 1) | 0)) {
        i2c_stop();
        return 0;
    }

    if (!i2c_write(SSD1327_CONTROL_CMD)) {
        i2c_stop();
        return 0;
    }

    if (!i2c_write(cmd)) {
        i2c_stop();
        return 0;
    }

    i2c_stop();
    return 1;
}

static uint8_t oled_write_command2(uint8_t cmd, uint8_t arg) {
    if (!i2c_start((OLED_I2C_ADDR << 1) | 0)) {
        i2c_stop();
        return 0;
    }

    if (!i2c_write(SSD1327_CONTROL_CMD) || !i2c_write(cmd) ||
        !i2c_write(SSD1327_CONTROL_CMD) || !i2c_write(arg)) {
        i2c_stop();
        return 0;
    }

    i2c_stop();
    return 1;
}

static void oled_write_data_chunk(const uint8_t *data, uint8_t len) {
    if (!i2c_start((OLED_I2C_ADDR << 1) | 0)) {
        i2c_stop();
        return;
    }

    if (!i2c_write(SSD1327_CONTROL_DATA)) {
        i2c_stop();
        return;
    }

    for (uint8_t i = 0; i < len; i++) {
        if (!i2c_write(data[i])) {
            break;
        }
    }

    i2c_stop();
}

// low level code for display
void oled_init(void) {
    OLED_RST_DDR |= (1 << OLED_RST_PIN);
    oled_rst_high();

    i2c_init();
    oled_reset();

    // init sequence pulled from Adafruit's SSD1327 128x128 init sequence (via Github)
    oled_write_command(SSD1327_DISPLAYOFF);
    oled_write_command2(SSD1327_SETCONTRAST, 0x80);
    oled_write_command2(SSD1327_SETREMAP, 0x51);
    oled_write_command2(SSD1327_SETSTARTLINE, 0x00);
    oled_write_command2(SSD1327_SETDISPLAYOFFSET, 0x00);
    oled_write_command(SSD1327_DISPLAYALLOFF);
    oled_write_command2(SSD1327_SETMULTIPLEX, 0x7F);
    oled_write_command2(SSD1327_PHASELEN, 0x11);
    oled_write_command2(SSD1327_DCLK, 0x00);
    oled_write_command2(SSD1327_REGULATOR, 0x01);
    oled_write_command2(SSD1327_PRECHARGE2, 0x04);
    oled_write_command2(SSD1327_SETVCOM, 0x0F);
    oled_write_command2(SSD1327_PRECHARGE, 0x08);
    oled_write_command2(SSD1327_FUNCSELB, 0x62);
    oled_write_command2(SSD1327_CMDLOCK, 0x12);
    oled_write_command(SSD1327_NORMALDISPLAY);

    oled_clear(0x0);
    oled_update();

    _delay_ms(100);
    oled_write_command(SSD1327_DISPLAYON);
}

void oled_clear(uint8_t shade) {
    shade &= 0x0F;
    uint8_t packed = (shade << 4) | shade;

    for (uint16_t i = 0; i < OLED_BUFFER_SIZE; i++) {
        oled_buffer[i] = packed;
    }
}

void oled_draw_pixel(uint8_t x, uint8_t y, uint8_t shade) {
    if (x >= OLED_WIDTH || y >= OLED_HEIGHT) {
        return;
    }

    shade &= 0x0F;

    uint16_t index = ((uint16_t)y * (OLED_WIDTH / 2)) + (x / 2);

    if ((x & 1) == 0) {
        oled_buffer[index] = (oled_buffer[index] & 0x0F) | (shade << 4);
    } else {
        oled_buffer[index] = (oled_buffer[index] & 0xF0) | shade;
    }
}

void oled_update(void) {
    // 128 pixels wide = 64 bytes wide
    oled_write_command2(SSD1327_SETROW, 0x00);
    oled_write_command(0x7F);

    oled_write_command2(SSD1327_SETCOLUMN, 0x00);
    oled_write_command(0x3F);

    // Send in 16-byte chunks to stay within small AVR TWI buffer assumptions
    for (uint16_t i = 0; i < OLED_BUFFER_SIZE; i += 16) {
        oled_write_data_chunk(&oled_buffer[i], 16);
    }
}


// for use to write to display
void display_draw_char(uint8_t x, uint8_t y, char c, uint8_t shade) {
    if (c < 32 || c > 126) {
        c = '?';
    }

    const uint8_t *glyph = font5x7[(uint8_t)c - 32];

    for (uint8_t col = 0; col < 5; col++) {
        uint8_t bits = glyph[col];
        for (uint8_t row = 0; row < 7; row++) {
            if (bits & (1 << row)) {
                oled_draw_pixel(x + col, y + row, shade);
            }
        }
    }
}

void display_draw_string(uint8_t x, uint8_t y, const char *str, uint8_t shade) {
    while (*str) {
        if (*str == '\n') {
            x = 0;
            y += 8;
        } else {
            display_draw_char(x, y, *str, shade);
            x += 6; // 5 pixels wide + 1 spacing
        }
        str++;
    }
}
