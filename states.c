#include "states.h"
#include "gps.h"
#include "display.h"
#include "buttons.h"
#include "power.h"
#include "waypoint.h"
#include "sos.h"
#include "battery.h"
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <util/delay.h>

void draw_string(uint8_t x, uint8_t y, const char *str, uint8_t scale, uint8_t color);

ISR(PCINT0_vect) {}

static DeviceState current_state  = STATE_INIT;
static DeviceState previous_state = STATE_INIT;
static DeviceState pre_low_power_state = STATE_IDLE;

// menu
#define MENU_ITEM_COUNT 3
static uint8_t menu_index = 0;
static uint8_t last_menu_index = 255;  // force first draw

void state_machine_set(DeviceState new_state) {
    current_state = new_state;
}

DeviceState state_machine_get(void) {
    return current_state;
}

void system_init(void) {
    oled_init();
    buttons_init();
    gps_init();
    power_init();
    battery_init();
    waypoint_init();
    sos_init();
}

void state_enter_low_power(DeviceState from_state) {
    pre_low_power_state = from_state;
    current_state = STATE_LOW_POWER;
}

// ── returns 1 if display needs redrawing ─────────────────────
static uint8_t state_changed(void) {
    return current_state != previous_state;
}

// ── draw functions — only called when needed ─────────────────

static void draw_idle(void) {
    oled_clear(0xF);
    draw_string(34, 15, "ready", 1, 0x0);
    draw_string(10, 35, "hold sos: emergency", 1, 0x0);
    draw_string(10, 55, "up/dn: open menu", 1, 0x0);
    draw_string(10, 75, "pwr: shutdown", 1, 0x0);
    oled_update();
}

static void draw_menu(void) {
    oled_clear(0xF);
    draw_string(40, 5, "menu", 1, 0x0);

    if (menu_index == 0) {
        draw_string(20, 30, "> battery",  1, 0x0);
    } else {
        draw_string(20, 30, "  battery",  1, 0x0);
    }

    if (menu_index == 1) {
        draw_string(20, 50, "> waypoint", 1, 0x0);
    } else {
        draw_string(20, 50, "  waypoint", 1, 0x0);
    }

    if (menu_index == 2) {
        draw_string(20, 70, "> shutdown", 1, 0x0);
    } else {
        draw_string(20, 70, "  shutdown", 1, 0x0);
    }

    draw_string(20, 100, "up/dn:scroll",  1, 0x0);
    draw_string(20, 113, "sos:select pwr:back", 1, 0x0);
    oled_update();
}

static void draw_battery_screen(void) {
    uint16_t mv  = battery_read_mv();
    uint8_t  pct = battery_percent();
    uint8_t  low = battery_is_low();

    char volt[8];
    char pctstr[6];
    char status[4];

    volt[0] = '0' + (mv / 1000);
    volt[1] = '.';
    volt[2] = '0' + ((mv % 1000) / 100);
    volt[3] = '0' + ((mv % 100)  / 10);
    volt[4] = 'v';
    volt[5] = '\0';

    pctstr[0] = '0' + (pct / 100);
    pctstr[1] = '0' + ((pct % 100) / 10);
    pctstr[2] = '0' + (pct % 10);
    pctstr[3] = '\0';

    if (low) {
        status[0]='l'; status[1]='o';
        status[2]='w'; status[3]='\0';
    } else {
        status[0]='o'; status[1]='k';
        status[2]='\0';
    }

    oled_clear(0xF);
    draw_string(22,  5, "battery info", 1, 0x0);
    draw_string(20, 25, "voltage: ",    1, 0x0);
    draw_string(74, 25, volt,           1, 0x0);
    draw_string(20, 45, "percent: ",    1, 0x0);
    draw_string(74, 45, pctstr,         1, 0x0);

    uint8_t sx = low ? 31 : 34;
    draw_string(sx,      65, "status: ", 1, 0x0);
    draw_string(sx + 48, 65, status,     1, 0x0);

    draw_string(20, 100, "pwr: back", 1, 0x0);
    oled_update();
}

#define RETRY_LIMIT       3
#define BATT_REFRESH_MS   2000

void state_machine_run(uint16_t elapsed_ms) {
    buttons_update(elapsed_ms);

    static uint8_t  flag_sos_rx   = 0;
    static uint16_t batt_timer    = 0;

    switch (current_state) {

        // ── INIT ──────────────────────────────────────────────
        case STATE_INIT:
            system_init();
            current_state = STATE_MENU;
            break;

        // ── IDLE ──────────────────────────────────────────────
        case STATE_IDLE:
            // only redraw when entering this state
            if (state_changed()) {
                draw_idle();
                previous_state = current_state;
            }

            if (power_is_low()) {
                state_enter_low_power(STATE_IDLE);
                break;
            }

            if (sos_hold_complete()) {
                current_state = STATE_GPS_CHECK;
                break;
            }

            if (button_pressed(ON_OFF)) {
                current_state = STATE_SHUTDOWN;
                break;
            }

            if (button_pressed(UP) || button_pressed(DOWN)) {
                menu_index = 0;
                last_menu_index = 255;  // force menu redraw
                current_state = STATE_MENU;
                break;
            }

            if (button_pressed(LEFT) || button_pressed(RIGHT)) {
                current_state = STATE_WAYPOINT_CONFIRM;
                break;
            }
            break;

        // ── MENU ──────────────────────────────────────────────
        case STATE_MENU:
            // redraw when entering state OR when selection changes
            if (state_changed() || menu_index != last_menu_index) {
                draw_menu();
                previous_state = current_state;
                last_menu_index = menu_index;
            }

            if (button_pressed(UP)) {
                if (menu_index > 0) {
                    menu_index--;
                } else {
                    menu_index = MENU_ITEM_COUNT - 1;
                }
                break;
            }

            if (button_pressed(DOWN)) {
                if (menu_index < MENU_ITEM_COUNT - 1) {
                    menu_index++;
                } else {
                    menu_index = 0;
                }
                break;
            }

            if (button_pressed(SOS)) {
                switch (menu_index) {
                    case 0:
                        batt_timer = BATT_REFRESH_MS; // force immediate draw
                        current_state = STATE_BATTERY_CHECK;
                        break;
                    case 1:
                        current_state = STATE_WAYPOINT_CONFIRM;
                        break;
                    case 2:
                        current_state = STATE_SHUTDOWN;
                        break;
                }
                break;
            }

            if (button_pressed(ON_OFF)) {
                current_state = STATE_IDLE;
                break;
            }
            break;

        // ── BATTERY CHECK ─────────────────────────────────────
        case STATE_BATTERY_CHECK:
            // redraw on entry and every BATT_REFRESH_MS
            batt_timer += elapsed_ms;
            if (state_changed() || batt_timer >= BATT_REFRESH_MS) {
                draw_battery_screen();
                previous_state = current_state;
                batt_timer = 0;
            }

            if (button_pressed(ON_OFF)) {
                current_state = STATE_MENU;
                break;
            }

            if (sos_hold_complete()) {
                current_state = STATE_GPS_CHECK;
                break;
            }
            break;

        // ── GPS CHECK ─────────────────────────────────────────
        case STATE_GPS_CHECK:
            if (state_changed()) {
                oled_clear(0xF);
                draw_string(10, 50, "acquiring gps...", 1, 0x0);
                oled_update();
                previous_state = current_state;
            }

            if (coordinates_valid && sos_hold_complete()) {
                current_state = STATE_SOS_ARMING;
            } else if (!coordinates_valid) {
                current_state = STATE_IDLE;
            }
            break;

        // ── SOS ARMING ────────────────────────────────────────
        case STATE_SOS_ARMING:
            if (state_changed()) {
                oled_clear(0xF);
                draw_string(10, 40, "sos arming!", 1, 0x0);
                draw_string(10, 60, "hold button...", 1, 0x0);
                oled_update();
                previous_state = current_state;
            }

            if (sos_time_remaining() > 0) {
                current_state = STATE_SOS_ARMING;
            } else if (sos_hold_complete()) {
                current_state = STATE_SOS_FORMAT;
            } else {
                current_state = STATE_IDLE;
            }
            break;

        // ── SOS FORMAT ────────────────────────────────────────
        case STATE_SOS_FORMAT:
            if (state_changed()) {
                oled_clear(0xF);
                draw_string(10, 50, "formatting...", 1, 0x0);
                oled_update();
                previous_state = current_state;
            }

            flag_sos_rx = 0;
            current_state = STATE_SOS_TRANSMIT;
            break;

        // ── SOS TRANSMIT ──────────────────────────────────────
        case STATE_SOS_TRANSMIT:
            if (state_changed()) {
                oled_clear(0xF);
                draw_string(10, 40, "sending sos...", 1, 0x0);
                draw_string(10, 60, "please wait", 1, 0x0);
                oled_update();
                previous_state = current_state;
            }

            if (ack_received) {
                current_state = STATE_SOS_SUCCESS;
            } else if (flag_sos_rx < RETRY_LIMIT && !ack_received) {
                flag_sos_rx++;
                current_state = STATE_SOS_TRANSMIT;
            } else if (flag_sos_rx >= RETRY_LIMIT) {
                flag_sos_rx = 0;
                current_state = STATE_SOS_FAILURE;
            }
            break;

        // ── SOS SUCCESS ───────────────────────────────────────
        case STATE_SOS_SUCCESS:
            if (state_changed()) {
                oled_clear(0xF);
                draw_string(10, 40, "help is coming!", 1, 0x0);
                draw_string(10, 60, "stay put", 1, 0x0);
                oled_update();
                previous_state = current_state;
                _delay_ms(3000);
            }
            current_state = STATE_IDLE;
            break;

        // ── SOS FAILURE ───────────────────────────────────────
        case STATE_SOS_FAILURE:
            if (state_changed()) {
                oled_clear(0xF);
                draw_string(10, 40, "send failed!", 1, 0x0);
                draw_string(10, 60, "hold sos: retry", 1, 0x0);
                draw_string(10, 80, "any btn: cancel", 1, 0x0);
                oled_update();
                previous_state = current_state;
            }

            if (sos_hold_complete()) {
                current_state = STATE_SOS_ARMING;
                break;
            }

            if (button_pressed(UP)   ||
                button_pressed(DOWN) ||
                button_pressed(LEFT) ||
                button_pressed(RIGHT)) {
                current_state = STATE_IDLE;
                break;
            }
            break;

        // ── LOW POWER ─────────────────────────────────────────
        case STATE_LOW_POWER: {
            if (state_changed()) {
                oled_clear(0xF);
                draw_string(22, 40, "low battery!", 1, 0x0);
                draw_string(10, 60, "sos still active", 1, 0x0);
                oled_update();
                previous_state = current_state;
                _delay_ms(2000);
            }

            gps_sleep();

            set_sleep_mode(SLEEP_MODE_PWR_SAVE);
            sleep_enable();
            sei();
            sleep_cpu();
            sleep_disable();

            if (sos_hold_complete()) {
                gps_wake();
                current_state = STATE_GPS_CHECK;
                break;
            }

            if (!power_is_low()) {
                gps_wake();
                current_state = pre_low_power_state;
                break;
            }

            current_state = STATE_LOW_POWER;
            break;
        }

        // ── ERROR ─────────────────────────────────────────────
        case STATE_ERROR: {
            if (state_changed()) {
                oled_clear(0xF);
                draw_string(34, 30, "system", 1, 0x0);
                draw_string(40, 50, "error!", 1, 0x0);
                draw_string(10, 75, "restarting...", 1, 0x0);
                oled_update();
                previous_state = current_state;
                _delay_ms(3000);
            }

            system_init();
            current_state = STATE_IDLE;
            break;
        }

        // ── SHUTDOWN ──────────────────────────────────────────
        case STATE_SHUTDOWN: {
            oled_clear(0xF);
            draw_string(28, 40, "shutting", 1, 0x0);
            draw_string(40, 60, "down...", 1, 0x0);
            oled_update();
            _delay_ms(2000);

            gps_sleep();
            oled_clear(0x0);
            oled_update();

            set_sleep_mode(SLEEP_MODE_PWR_DOWN);
            sleep_enable();

            cli();
            PCMSK0 = (1 << PCINT5) | (1 << PCINT0);
            PCICR  = (1 << PCIE0);
            sei();

            sleep_cpu();

            sleep_disable();
            PCICR  = 0;
            PCMSK0 = 0;
            _delay_ms(50);

            system_init();
            previous_state = STATE_INIT; // force redraw after wake

            if (!(PINA & (1 << PA0))) {
                current_state = STATE_GPS_CHECK;
            } else {
                current_state = STATE_IDLE;
            }
            break;
        }

        // ── WAYPOINT CONFIRM ──────────────────────────────────
        case STATE_WAYPOINT_CONFIRM:
            if (state_changed()) {
                oled_clear(0xF);
                draw_string(10, 40, "save waypoint?", 1, 0x0);
                draw_string(10, 60, "sos: yes", 1, 0x0);
                draw_string(10, 80, "pwr: cancel", 1, 0x0);
                oled_update();
                previous_state = current_state;
            }

            if (button_pressed(SOS)) {
                waypoint_save();
                current_state = STATE_WAYPOINT_SAVED;
                break;
            }

            if (button_pressed(ON_OFF)) {
                current_state = STATE_IDLE;
                break;
            }
            break;

        // ── WAYPOINT SAVED ────────────────────────────────────
        case STATE_WAYPOINT_SAVED:
            if (state_changed()) {
                oled_clear(0xF);
                draw_string(10, 50, "waypoint saved!", 1, 0x0);
                oled_update();
                previous_state = current_state;
                _delay_ms(1500);
            }
            current_state = STATE_IDLE;
            break;

        // ── WAYPOINT SELECT/DELETE ────────────────────────────
        case STATE_WAYPOINT_SELECT_DELETE:
            if (state_changed()) {
                oled_clear(0xF);
                draw_string(10, 20, "waypoints", 1, 0x0);
                draw_string(10, 40, "up/dn: scroll", 1, 0x0);
                draw_string(10, 60, "sos: select", 1, 0x0);
                draw_string(10, 80, "pwr: back", 1, 0x0);
                oled_update();
                previous_state = current_state;
            }

            if (button_pressed(UP)) {
                break;
            }

            if (button_pressed(DOWN)) {
                break;
            }

            if (button_pressed(ON_OFF)) {
                current_state = STATE_IDLE;
                break;
            }
            break;
    }
}
