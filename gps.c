// gps.c
#include "gps.h"
#include <avr/io.h>
#include <stdlib.h>
#include <string.h>

#define BAUD 9600
#ifndef F_CPU
#define F_CPU 8000000UL
#endif
#define UBRR_VAL (F_CPU/16/BAUD - 1)

// Initialize UART for GPS communication
void gps_init(void) {
    // Set baud rate
    UBRR0H = (uint8_t)(UBRR_VAL >> 8);
    UBRR0L = (uint8_t)(UBRR_VAL);

    // Enable receiver and transmitter
    UCSR0B = (1 << RXEN0) | (1 << TXEN0);

    // Set frame format: 8 data bits, 1 stop bit, no parity
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

// Read a single character from UART
static char uart_read(void) {
    while (!(UCSR0A & (1 << RXC0)));  // wait until data received
    return UDR0;
}

// Read a full NMEA sentence into buffer, returns 1 if successful
static uint8_t read_sentence(char *buf, uint8_t maxlen) {
    uint8_t i = 0;
    char c;

    // Wait for start of sentence '$'
    while (uart_read() != '$');
    buf[i++] = '$';

    // Read until newline or buffer full
    while (i < maxlen - 1) {
        c = uart_read();
        if (c == '\n') break;
        buf[i++] = c;
    }
    buf[i] = '\0';
    return 1;
}

// Parse a comma separated field from NMEA sentence
// field 0 = sentence type, field 1 = first data field, etc.
static void get_field(const char *sentence, uint8_t field, char *out, uint8_t outlen) {
    uint8_t f = 0, i = 0;
    const char *p = sentence;

    while (*p) {
        if (*p == ',') {
            f++;
            p++;
            continue;
        }
        if (f == field) {
            while (*p && *p != ',' && i < outlen - 1) {
                out[i++] = *p++;
            }
            break;
        }
        p++;
    }
    out[i] = '\0';
}

// Convert NMEA lat/lon format (DDMM.MMMM) to decimal degrees
static float nmea_to_decimal(const char *val, const char *dir) {
    if (val[0] == '\0') return 0.0;

    float raw = atof(val);
    int degrees = (int)(raw / 100);
    float minutes = raw - (degrees * 100);
    float decimal = degrees + (minutes / 60.0);

    if (dir[0] == 'S' || dir[0] == 'W') decimal = -decimal;
    return decimal;
}

// Read GPS and fill in a GPS_Data struct
// Returns 1 if a valid fix was obtained, 0 otherwise
uint8_t gps_read(GPS_Data *data) {
    char sentence[82];   // max NMEA sentence length is 82 chars
    char field[12];

    while (1) {
        read_sentence(sentence, sizeof(sentence));

        // Look for $GPRMC or $GNRMC sentence
        if (strncmp(sentence, "$GPRMC", 6) != 0 &&
            strncmp(sentence, "$GNRMC", 6) != 0) {
            continue;   // not the sentence we want, try again
        }

        // Field 2 is status: 'A' = active/valid fix, 'V' = void/no fix
        get_field(sentence, 2, field, sizeof(field));
        if (field[0] != 'A') return 0;   // no fix

        // Field 3 = latitude value, field 4 = N/S
        char lat_val[12], lat_dir[2];
        get_field(sentence, 3, lat_val, sizeof(lat_val));
        get_field(sentence, 4, lat_dir, sizeof(lat_dir));
        data->latitude = nmea_to_decimal(lat_val, lat_dir);

        // Field 5 = longitude value, field 6 = E/W
        char lon_val[12], lon_dir[2];
        get_field(sentence, 5, lon_val, sizeof(lon_val));
        get_field(sentence, 6, lon_dir, sizeof(lon_dir));
        data->longitude = nmea_to_decimal(lon_val, lon_dir);

        // Field 1 = UTC time (HHMMSS.SS)
        get_field(sentence, 1, field, sizeof(field));
        data->hour   = (field[0]-'0')*10 + (field[1]-'0');
        data->minute = (field[2]-'0')*10 + (field[3]-'0');
        data->second = (field[4]-'0')*10 + (field[5]-'0');

        return 1;   // valid fix
    }
}
