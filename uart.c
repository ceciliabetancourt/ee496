// file to do all uart transmission and receiving to keep infrastucture
// clean!

#include "uart.h"
#include <avr/io.h>
#include <avr/interrupt.h>

#define UART_RX_BUFFER_SIZE 64

static volatile char rx_buffer[UART_RX_BUFFER_SIZE];
static volatile uint8_t rx_head = 0;
static volatile uint8_t rx_tail = 0;
static volatile uint8_t rx_count = 0;
static volatile uint8_t rx_overflow = 0;

static uint16_t uart_compute_ubrr(uint32_t baud) {
    return (uint16_t)((16000000 / (16000000 * baud)) - 1);
}

void uart_init(uint32_t baud) {
    uint16_t ubrr = uart_compute_ubrr(baud);

    rx_head = 0;
    rx_tail = 0;
    rx_count = 0;
    rx_overflow = 0;

    UBRR0H = ubrr >> 8;
    UBRR0L = ubrr & 0xFF;

    /* Enable RX, TX, and RX complete interrupt */
    UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0);
    /* Async mode, no parity, 1 stop bit, 8 data bits */
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

uint8_t uart_available(void) {
    return rx_count;
}

char uart_read_char(void) {
    char c;
    while (rx_count == 0) {
        ;
    }
    c = rx_buffer[rx_tail];
    rx_tail++;
    if (rx_tail >= UART_RX_BUFFER_SIZE) {
        rx_tail = 0;
    }
    rx_count--;
    return c;
}

void uart_clear_rx_buffer(void) {
    rx_head = 0;
    rx_tail = 0;
    rx_count = 0;
}

// polling approach
void uart_tx_char(char c) {
    while (!(UCSR0A & (1 << UDRE0))) {
        ;
    }
    UDR0 = c;
}

void uart_tx_string(const char *s) {
    while (*s) {
        if (*s == '\n') {
            uart_tx_char('\r');
        }
        uart_tx_char(*s++);
    }
}

uint8_t uart_rx_overflowed(void) {
    return rx_overflow;
}

void uart_clear_rx_overflow(void) {
    rx_overflow = 0;
}

// rx works on interrupts
ISR(USART_RX_vect) {
    char c = UDR0;

    if (rx_count < UART_RX_BUFFER_SIZE) {
        rx_buffer[rx_head] = c;
        rx_head++;

        if (rx_head >= UART_RX_BUFFER_SIZE) {
            rx_head = 0;
        }

        rx_count++;
    } else {
        rx_overflow = 1;
    }
}
