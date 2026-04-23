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

static DeviceState current_state   = STATE_INIT;
static DeviceState previous_state  = STATE_INIT;
static DeviceState pre_low_power_state = STATE_MENU;

// ── menu ──────────────────────────────────────────────────────
#define MENU_ITEM_COUNT  3
#define MENU_BATTERY     0
#define MENU_WAYPOINT_SAVE  1
#define MENU_WAYPOINT_LIST  2

static uint8_t menu_index      = 0;
static uint8_t last_menu_index = 255;

// ── waypoint list ─────────────────────────────────────────────
#define MAX_WAYPOINTS 5
static uint8_t waypoint_list_index = 0;

// ── countdown ─────────────────────────────────────────────────
#define COUNTDOWN_START  3
static uint8_t  countdown_value  = COUNTDOWN_START;
static uint16_t countdown_timer  = 0;
#define COUNTDOWN_STEP_MS 1000

// ── battery refresh ───────────────────────────────────────────
#define BATT_REFRESH_MS 2000
static uint16_t batt_timer = 0;

// ── retry limit ───────────────────────────────────────────────
#define RETRY_LIMIT 3

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

static uint8_t state_changed(void) {
    return current_state != previous_state;
}

// ── draw helpers ──────────────────────────────────────────────

static void draw_menu(void) {
    oled_clear(0xF);
    draw_string(34, 5, "menu", 1, 0x0);

    // item 0: battery
    if (menu_index == MENU_BATTERY) {
        draw_string(20, 28, "> battery info", 1, 0x0);
    } else {
        draw_string(20, 28, "  battery info", 1, 0x0);
    }

    // item 1: waypoint save
    if (menu_index == MENU_WAYPOINT_SAVE) {
        draw_string(20, 48, "> save waypoint", 1, 0x0);
    } else {
        draw_string(20, 48, "  save waypoint", 1, 0x0);
    }

    // item 2: waypoint list
    if (menu_index == MENU_WAYPOINT_LIST) {
        draw_string(20, 68, "> waypoint list", 1, 0x0);
    } else {
        draw_string(20, 68, "  waypoint list", 1, 0x0);
    }

    draw_string(20, 95,  "up/dn: scroll",   1, 0x0);
    draw_string(20, 108, "right: select",   1, 0x0);
    oled_update();
}

static void draw_battery(void) {
    uint16_t mv  = battery_read_mv();
    uint8_t  pct = battery_percent();
    uint8_t  low = battery_is_low();

    char volt[8];
    char pctstr[5];
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
    draw_string(22,  5, "battery info",  1, 0x0);
    draw_string(20, 30, "voltage: ",     1, 0x0);
    draw_string(74, 30, volt,            1, 0x0);
    draw_string(20, 50, "percent: ",     1, 0x0);
    draw_string(74, 50, pctstr,          1, 0x0);

    uint8_t sx = low ? 20 : 20;
    draw_string(sx,      70, "status: ", 1, 0x0);
    draw_string(sx + 48, 70, status,     1, 0x0);

    draw_string(20, 105, "left: back",   1, 0x0);
    oled_update();
}

static void draw_countdown(void) {
    oled_clear(0xF);
    draw_string(10, 20, "sos confirmed!", 1, 0x0);
    draw_string(10, 45, "sending in:", 1, 0x0);

    // draw countdown number large (scale 3)
    char num[2];
    num[0] = '0' + countdown_value;
    num[1] = '\0';
    draw_string(55, 65, num, 3, 0x0);

    draw_string(10, 108, "left: cancel", 1, 0x0);
    oled_update();
}

static void draw_waypoint_list(uint8_t index) {
    // TODO: replace with real waypoint data from waypoint.c
    // for now shows index number as placeholder
    char idx[3];
    idx[0] = '0' + index;
    idx[1] = '\0';

    oled_clear(0xF);
    draw_string(20,   5, "waypoint list",  1, 0x0);
    draw_string(20,  30, "waypoint: ",     1, 0x0);
    draw_string(80,  30, idx,              1, 0x0);
    draw_string(20,  55, "up/dn: scroll",  1, 0x0);
    draw_string(20,  75, "right: navigate",1, 0x0);
    draw_string(20,  95, "left: back",     1, 0x0);
    oled_update();
}

// ── state machine ─────────────────────────────────────────────

void state_machine_run(uint16_t elapsed_ms) {
    buttons_update(elapsed_ms);

    static uint8_t flag_sos_rx = 0;

    switch (current_state) {

        // ── INIT ──────────────────────────────────────────────
        case STATE_INIT:
            system_init();
            menu_index      = 0;
            last_menu_index = 255;
            current_state   = STATE_MENU;
            break;

        // ── MENU (home screen) ────────────────────────────────
        case STATE_MENU:
            if (state_changed() || menu_index != last_menu_index) {
                draw_menu();
                previous_state  = current_state;
                last_menu_index = menu_index;
            }

            if (power_is_low()) {
                state_enter_low_power(STATE_MENU);
                break;
            }

            // SOS held 3s → confirmation screen
            if (sos_hold_complete()) {
                current_state = STATE_SOS_CONFIRM;
                break;
            }

            // PWR → shutdown
            if (button_pressed(ON_OFF)) {
                current_state = STATE_SHUTDOWN;
                break;
            }

            // UP scrolls up — wraps
            if (button_pressed(UP)) {
                menu_index = (menu_index > 0)
                    ? menu_index - 1
                    : MENU_ITEM_COUNT - 1;
                break;
            }

            // DOWN scrolls down — wraps
            if (button_pressed(DOWN)) {
                menu_index = (menu_index < MENU_ITEM_COUNT - 1)
                    ? menu_index + 1
                    : 0;
                break;
            }

            // RIGHT selects
            if (button_pressed(RIGHT)) {
                switch (menu_index) {
                    case MENU_BATTERY:
                        batt_timer = BATT_REFRESH_MS; // force immediate draw
                        current_state = STATE_BATTERY_CHECK;
                        break;
                    case MENU_WAYPOINT_SAVE:
                        current_state = STATE_WAYPOINT_SAVE;
                        break;
                    case MENU_WAYPOINT_LIST:
                        waypoint_list_index = 0;
                        current_state = STATE_WAYPOINT_LIST;
                        break;
                }
                break;
            }

            // LEFT does nothing on menu — already home
            break;

        // ── BATTERY CHECK ─────────────────────────────────────
        case STATE_BATTERY_CHECK:
            batt_timer += elapsed_ms;
            if (state_changed() || batt_timer >= BATT_REFRESH_MS) {
                draw_battery();
                previous_state = current_state;
                batt_timer = 0;
            }

            // LEFT → back to menu
            if (button_pressed(LEFT)) {
                current_state = STATE_MENU;
                break;
            }

            // SOS still works from anywhere
            if (sos_hold_complete()) {
                current_state = STATE_SOS_CONFIRM;
                break;
            }
            break;

        // ── SOS CONFIRM ───────────────────────────────────────
        case STATE_SOS_CONFIRM:
            if (state_changed()) {
                oled_clear(0xF);
                draw_string(10, 25, "are you sure?",  1, 0x0);
                draw_string(10, 50, "right: yes send sos", 1, 0x0);
                draw_string(10, 75, "left: cancel",   1, 0x0);
                oled_update();
                previous_state = current_state;
            }

            // RIGHT confirms → start countdown
            if (button_pressed(RIGHT)) {
                countdown_value = COUNTDOWN_START;
                countdown_timer = 0;
                current_state   = STATE_SOS_COUNTDOWN;
                break;
            }

            // LEFT cancels → back to menu
            if (button_pressed(LEFT)) {
                current_state = STATE_MENU;
                break;
            }
            break;

        // ── SOS COUNTDOWN ─────────────────────────────────────
        case STATE_SOS_COUNTDOWN:
            // draw on entry and every second
            countdown_timer += elapsed_ms;

            if (state_changed()) {
                draw_countdown();
                previous_state = current_state;
            }

            if (countdown_timer >= COUNTDOWN_STEP_MS) {
                countdown_timer = 0;

                if (countdown_value > 0) {
                    countdown_value--;
                    draw_countdown();  // redraw with new number
                }

                if (countdown_value == 0) {
                    // countdown done — proceed to GPS check
                    current_state = STATE_GPS_CHECK;
                }
            }

            // LEFT still cancels during countdown
            if (button_pressed(LEFT)) {
                current_state = STATE_MENU;
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

            if (coordinates_valid && button_down(SOS)) {
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
                oled_update();
                previous_state = current_state;
            }

            if (button_down(SOS)) {
                oled_clear(0xF);
                if(button_hold_time(SOS) < 1000) {
                    oled_clear(0xF);
                    draw_string(10, 50, "3 seconds remaining", 1, 0x0);
                    oled_update();
                }
                else if((button_hold_time(SOS) > 1000) && (button_hold_time(SOS) < 2000)) {
                    oled_clear(0xF);
                    draw_string(10, 50, "2 seconds remaining", 1, 0x0);
                    oled_update();
                }
                else if((button_hold_time(SOS) > 2000) && (button_hold_time(SOS) < 3000)) {
                    oled_clear(0xF);
                    draw_string(10, 50, "1 second remaining", 1, 0x0);
                    oled_update();
                }
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
                draw_string(10, 50, "formatting msg...", 1, 0x0);
                oled_update();
                previous_state = current_state;
            }

            // TODO: iridium_format() in sos.c
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

            // TODO: iridium_send_sos() in sos.c
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
                draw_string(10, 35, "sos sent!", 1, 0x0);
                draw_string(10, 55, "help is coming!", 1, 0x0);
                draw_string(10, 75, "stay put", 1, 0x0);
                oled_update();
                previous_state = current_state;
                _delay_ms(3000);
            }
            current_state = STATE_MENU;
            break;

        // ── SOS FAILURE ───────────────────────────────────────
        case STATE_SOS_FAILURE:
            if (state_changed()) {
                oled_clear(0xF);
                draw_string(10, 35, "send failed!", 1, 0x0);
                draw_string(10, 55, "sos: retry", 1, 0x0);
                draw_string(10, 75, "left: cancel", 1, 0x0);
                oled_update();
                previous_state = current_state;
            }

            // SOS held again → retry
            if (sos_hold_complete()) {
                current_state = STATE_SOS_CONFIRM;
                break;
            }

            // LEFT → back to menu
            if (button_pressed(LEFT)) {
                current_state = STATE_MENU;
                break;
            }
            break;

        // ── LOW POWER ─────────────────────────────────────────
        case STATE_LOW_POWER: {
            if (state_changed()) {
                oled_clear(0xF);
                draw_string(22, 40, "low battery!", 1, 0x0);
                draw_string(10, 60, "sos still works", 1, 0x0);
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
                current_state = STATE_SOS_CONFIRM;
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
            current_state = STATE_MENU;
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
            // PA0 (SOS) = PCINT0, PA5 (ON_OFF/PWR) = PCINT5
            PCMSK0 = (1 << PCINT5) | (1 << PCINT0);
            PCICR  = (1 << PCIE0);
            sei();

            sleep_cpu();

            sleep_disable();
            PCICR  = 0;
            PCMSK0 = 0;
            _delay_ms(50);

            system_init();
            previous_state  = STATE_INIT;  // force redraw after wake
            menu_index      = 0;
            last_menu_index = 255;

            // SOS woke us → skip confirmation, go straight to GPS
            if (!(PINA & (1 << PA0))) {
                current_state = STATE_GPS_CHECK;
            } else {
                // PWR woke us → normal boot to menu
                current_state = STATE_MENU;
            }
            break;
        }

        // ── WAYPOINT SAVE ─────────────────────────────────────
        case STATE_WAYPOINT_SAVE:
            if (state_changed()) {
                oled_clear(0xF);
                draw_string(10, 30, "save waypoint?",  1, 0x0);
                draw_string(10, 55, "right: confirm",  1, 0x0);
                draw_string(10, 75, "left: cancel",    1, 0x0);
                oled_update();
                previous_state = current_state;
            }

            // RIGHT confirms save
            if (button_pressed(RIGHT)) {
                waypoint_save();
                current_state = STATE_WAYPOINT_SAVED;
                break;
            }

            // LEFT cancels
            if (button_pressed(LEFT)) {
                current_state = STATE_MENU;
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
            current_state = STATE_MENU;
            break;

        // ── WAYPOINT LIST ─────────────────────────────────────
        case STATE_WAYPOINT_LIST:
            if (state_changed()) {
                draw_waypoint_list(waypoint_list_index);
                previous_state = current_state;
            }

            // UP scrolls up through waypoints
            if (button_pressed(UP)) {
                if (waypoint_list_index > 0) {
                    waypoint_list_index--;
                    draw_waypoint_list(waypoint_list_index);
                }
                break;
            }

            // DOWN scrolls down through waypoints
            if (button_pressed(DOWN)) {
                if (waypoint_list_index < MAX_WAYPOINTS - 1) {
                    waypoint_list_index++;
                    draw_waypoint_list(waypoint_list_index);
                }
                break;
            }

            // RIGHT navigates to selected waypoint
            // TODO: implement waypoint navigation display
            if (button_pressed(RIGHT)) {
                break;
            }

            // LEFT → back to menu
            if (button_pressed(LEFT)) {
                current_state = STATE_MENU;
                break;
            }
            break;
    }
}
