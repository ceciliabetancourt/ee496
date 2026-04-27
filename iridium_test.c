#include "display.h"
#include <avr/io.h>
#include <util/delay.h>
#include <string.h>

#define IRIDIUM_BAUD  19200UL
#define UBRR1_VAL     (F_CPU / 16 / IRIDIUM_BAUD - 1)

#define SLP_PIN  PB3
#define CTS_PIN  PB1
#define NET_PIN  PB2

#define TIMEOUT_SHORT  500000UL
#define TIMEOUT_LONG   250000000UL   // ~8.5 min at 7.3728 MHz (SBDIX can need 5+ min)

static void iridium_init(void) {
    DDRB  |=  (1 << SLP_PIN);
    PORTB |=  (1 << SLP_PIN);
    DDRB  &= ~((1 << CTS_PIN) | (1 << NET_PIN));
    PORTB |=   (1 << CTS_PIN) | (1 << NET_PIN);

    UBRR1H = (uint8_t)(UBRR1_VAL >> 8);
    UBRR1L = (uint8_t)(UBRR1_VAL);
    UCSR1B = (1 << RXEN1) | (1 << TXEN1);
    UCSR1C = (1 << UCSZ11) | (1 << UCSZ10);
}

static void uart1_tx(char c) {
    while (!(UCSR1A & (1 << UDRE1)));
    UDR1 = c;
}

static void uart1_tx_str(const char *s) {
    while (*s) uart1_tx(*s++);
}

static void flush_rx(void) {
    while (UCSR1A & (1 << RXC1)) { volatile uint8_t x = UDR1; (void)x; }
}

static void send_cmd(const char *cmd) {
    flush_rx();
    uart1_tx_str(cmd);
    uart1_tx('\r');
}

static char rx_byte(uint32_t timeout) {
    uint32_t t = 0;
    while (!(UCSR1A & (1 << RXC1))) {
        if (++t >= timeout) return 0;
    }
    return UDR1;
}

static void read_resp(uint32_t idle_timeout, uint8_t max_total) {
    uint8_t total = 0;
    char c;
    while ((c = rx_byte(idle_timeout)) && (!max_total || total < max_total))
        total++;
}

static void connection_test(void) {
    uint8_t cts_ok  = !(PINB & (1 << CTS_PIN));  // low = modem ready
    uint8_t net_low = !(PINB & (1 << NET_PIN));   // low = network registered

    // Send AT and scan for "ok" in response
    flush_rx();
    uart1_tx_str("AT\r");
    uint8_t uart_ok = 0;
    uint8_t match = 0;
    for (uint8_t i = 0; i < 64; i++) {
        char c = rx_byte(TIMEOUT_SHORT);
        if (!c) break;
        if (c >= 'A' && c <= 'Z') c += 32;
        if      (c == 'o' && match == 0) match = 1;
        else if (c == 'k' && match == 1) { uart_ok = 1; break; }
        else    match = (c == 'o') ? 1 : 0;
    }

    oled_clear(0x0);
    draw_string(2, 10, "conn test:", 1, 0xF);
    draw_string(2, 28, uart_ok ? "uart: ok"    : "uart: fail!", 1, 0xF);
    draw_string(2, 44, cts_ok  ? "cts: low ok" : "cts: high!", 1, 0xF);
    draw_string(2, 60, net_low ? "net: low ok" : "net: high", 1, 0xF);
    oled_update();
    _delay_ms(4000);
}

int main(void) {
    oled_init();
    iridium_init();

    oled_clear(0x0);
    draw_string(2, 45, "booting...", 1, 0xF);
    oled_update();
    _delay_ms(5000);

    connection_test();

    oled_clear(0x0);
    draw_string(2, 45, "echo off", 1, 0xF);
    oled_update();
    send_cmd("ATE0");
    read_resp(TIMEOUT_SHORT, 128);

    oled_clear(0x0);
    draw_string(2, 45, "clear buf", 1, 0xF);
    oled_update();
    send_cmd("AT+SBDD0");
    read_resp(TIMEOUT_SHORT, 64);

    oled_clear(0x0);
    draw_string(2, 45, "writing msg", 1, 0xF);
    oled_update();
    send_cmd("AT+SBDWT=hello world");
    read_resp(TIMEOUT_SHORT, 128);

    oled_clear(0x0);
    draw_string(2, 45, "sending...", 1, 0xF);
    oled_update();
    send_cmd("AT+SBDIX");

    // Capture raw response so we can see exactly what the modem sends
    char raw[13] = {0};
    uint8_t ri = 0;
    while (ri < 12) {
        char c = rx_byte(TIMEOUT_LONG);
        if (!c) break;
        if (c >= 'A' && c <= 'Z') c += 32;
        if (c >= 0x20 && c != ',') raw[ri++] = c;
    }
    raw[ri] = '\0';

    oled_clear(0x0);
    draw_string(2, 45, raw, 1, 0xF);
    oled_update();

    while (1) {}
}
