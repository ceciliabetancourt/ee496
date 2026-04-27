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
#include <string.h>

void draw_string(uint8_t x, uint8_t y, const char *str, uint8_t scale, uint8_t color);

ISR(PCINT0_vect) {}

static DeviceState current_state       = STATE_INIT;
static DeviceState previous_state      = STATE_INIT;
static DeviceState pre_low_power_state = STATE_MENU;

// ── persistent SOS flag ───────────────────────────────────────
static uint8_t sos_triggered = 0;

// ── menu ──────────────────────────────────────────────────────
#define MENU_ITEM_COUNT      4
#define MENU_BATTERY         0
#define MENU_GPS             1
#define MENU_WAYPOINT_SAVE   2
#define MENU_WAYPOINT_LIST   3

static uint8_t menu_index      = 0;
static uint8_t last_menu_index = 255;

// ── waypoint list ─────────────────────────────────────────────
#define MAX_WAYPOINTS        5
static uint8_t waypoint_list_index = 0;

// ── countdown ─────────────────────────────────────────────────
#define COUNTDOWN_START      3
static uint8_t  countdown_value = COUNTDOWN_START;
static uint16_t countdown_timer = 0;
#define COUNTDOWN_STEP_MS    1000

#define SOS_HOLD_COUNTDOWN_START 3
static uint8_t  sos_hold_countdown_value = SOS_HOLD_COUNTDOWN_START;
static uint16_t sos_hold_countdown_timer = 0;

// ── battery refresh ───────────────────────────────────────────
#define BATT_REFRESH_MS      2000
static uint16_t batt_timer = 0;

// ── GPS view refresh ──────────────────────────────────────────
#define GPS_REFRESH_MS       1000
static uint16_t gps_view_timer = 0;
static uint8_t coordinates_valid = 0;
static GPS_Data current_gps_data;

// ── GPS check timeout ─────────────────────────────────────────
#define GPS_TIMEOUT_MS       60000   // 60 seconds
static uint32_t gps_timeout = 0;

#define RETRY_LIMIT          3

// ── waypoint save pending GPS ─────────────────────────────────
static GPS_Data wp_save_gps;
static uint8_t  wp_save_valid = 0;
static uint8_t  wp_slot_index = 0;

// ── waypoint navigation ───────────────────────────────────────
#define NAV_REFRESH_MS       60000
static uint16_t nav_timer      = 0;
static uint8_t  nav_wp_index   = 0;

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

// ── float to string (4 decimal places) ───────────────────────
static void float_to_str(float val, char *buf) {
    uint8_t i = 0;

    if (val < 0) {
        buf[i++] = '-';
        val = -val;
    }

    uint16_t int_part = (uint16_t)val;
    uint16_t dec_part = (uint16_t)((val - int_part) * 10000 + 0.5);

    if (dec_part >= 10000) {
        int_part++;
        dec_part = 0;
    }

    if (int_part >= 100) buf[i++] = '0' + (int_part / 100);
    if (int_part >= 10)  buf[i++] = '0' + ((int_part % 100) / 10);
    buf[i++] = '0' + (int_part % 10);

    buf[i++] = '.';
    buf[i++] = '0' + (dec_part / 1000);
    buf[i++] = '0' + ((dec_part % 1000) / 100);
    buf[i++] = '0' + ((dec_part % 100) / 10);
    buf[i++] = '0' + (dec_part % 10);
    buf[i] = '\0';
}

// ── draw functions ────────────────────────────────────────────

static void draw_menu(void) {
    oled_clear(0xF);
    draw_string(34, 5, "menu", 1, 0x0);
    draw_string(20, 28, menu_index == MENU_BATTERY      ? "> battery info"  : "  battery info",  1, 0x0);
    draw_string(20, 46, menu_index == MENU_GPS          ? "> gps coords"    : "  gps coords",    1, 0x0);
    draw_string(20, 64, menu_index == MENU_WAYPOINT_SAVE? "> save waypoint" : "  save waypoint", 1, 0x0);
    draw_string(20, 82, menu_index == MENU_WAYPOINT_LIST? "> waypoint list" : "  waypoint list", 1, 0x0);
    draw_string(20, 105, "up/down: scroll",  1, 0x0);
    draw_string(30, 116, "right: select",  1, 0x0);
    oled_update();
}

static void draw_battery(void) {
    uint16_t mv  = battery_read_mv();
    uint8_t  pct = battery_percent();
    uint8_t  low = battery_is_low();
    char volt[8], pctstr[5], status[4];

    volt[0]='0'+(mv/1000); volt[1]='.';
    volt[2]='0'+((mv%1000)/100);
    volt[3]='0'+((mv%100)/10);
    volt[4]='v'; volt[5]='\0';

    pctstr[0]='0'+(pct/100);
    pctstr[1]='0'+((pct%100)/10);
    pctstr[2]='0'+(pct%10);
    pctstr[3]='\0';

    if (low) { status[0]='l'; status[1]='o'; status[2]='w'; status[3]='\0'; }
    else     { status[0]='o'; status[1]='k'; status[2]='\0'; }

    oled_clear(0xF);
    draw_string(22,  5, "battery info", 1, 0x0);
    draw_string(20, 30, "voltage: ",    1, 0x0);
    draw_string(74, 30, volt,           1, 0x0);
    draw_string(20, 50, "percent: ",    1, 0x0);
    draw_string(74, 50, pctstr,         1, 0x0);
    draw_string(20, 70, "status: ",     1, 0x0);
    draw_string(68, 70, status,         1, 0x0);
    draw_string(20, 105, "left: back",  1, 0x0);
    oled_update();
}

static void draw_gps_view(void) {
    char lat_str[14], lon_str[14], time_str[10];
    oled_clear(0xF);
    draw_string(22, 5, "gps coords", 1, 0x0);

    draw_string(10, 45, "acquiring fix...", 1, 0x0);
    draw_string(10, 65, "please wait",      1, 0x0);
    oled_update();

    coordinates_valid = gps_read(&current_gps_data);

    oled_clear(0xF);
    draw_string(22, 5, "gps coords", 1, 0x0);

    if (!coordinates_valid) {
        draw_string(10, 45, "acquiring fix...", 1, 0x0);
        draw_string(10, 65, "please wait",      1, 0x0);
    } else {
        float_to_str(current_gps_data.latitude,  lat_str);
        float_to_str(current_gps_data.longitude, lon_str);
        draw_string(20, 28, "lat: ", 1, 0x0); 
        draw_string(50, 28,  lat_str, 1, 0x0);
        draw_string(20, 48, "lon: ", 1, 0x0); 
        draw_string(50, 48, lon_str, 1, 0x0);
        int hour = current_gps_data.hour;
        int minute = current_gps_data.minute;
        int second = current_gps_data.second;
        
        // Convert UTC → PDT
        hour -= 7;
        if (hour < 0) {
            hour += 24;
        }
        // Format string
        time_str[0] = '0' + (hour / 10);
        time_str[1] = '0' + (hour % 10);
        time_str[2] = ':';
        time_str[3] = '0' + (minute / 10);
        time_str[4] = '0' + (minute % 10);
        time_str[5] = ':';
        time_str[6] = '0' + (second / 10);
        time_str[7] = '0' + (second % 10);
        time_str[8] = '\0';
        draw_string(20, 68, "pdt: ",   1, 0x0); draw_string(50, 68, time_str, 1, 0x0);
        draw_string(20, 88, "fix: ok", 1, 0x0);
    }
    draw_string(20, 108, "left: back", 1, 0x0);
    oled_update();
}

static void draw_countdown(void) {
    oled_clear(0xF);
    draw_string(10, 20, "sos confirmed!", 1, 0x0);
    draw_string(10, 45, "sending in:",    1, 0x0);
    char num[2]; num[0]='0'+countdown_value; num[1]='\0';
    draw_string(55, 65, num, 3, 0x0);
    draw_string(10, 108, "left: cancel",  1, 0x0);
    oled_update();
}

static void draw_sos_countdown(void) {
    oled_clear(0xF);
    draw_string(10, 20, "sos button held", 1, 0x0);
    draw_string(15, 45, "entering sos in:", 1, 0x0);
    char num[2];
    num[0] = '0' + sos_hold_countdown_value;
    num[1] = '\0';
    draw_string(55, 65, num, 3, 0x0);
    draw_string(12, 108, "release to cancel", 1, 0x0);
    oled_update();
}

static void draw_gps_check(uint32_t timeout_ms) {
    char wait_str[5];
    uint8_t secs = (uint8_t)(timeout_ms / 1000);
    wait_str[0] = '0' + (secs / 10);
    wait_str[1] = '0' + (secs % 10);
    wait_str[2] = 's';
    wait_str[3] = '\0';

    oled_clear(0xF);
    draw_string(10, 25, "acquiring gps...", 1, 0x0);
    draw_string(10, 45, "elapsed: ",        1, 0x0);
    draw_string(64, 45, wait_str,           1, 0x0);
    draw_string(10, 65, "timeout: 60s",     1, 0x0);
    draw_string(10, 85, "left: cancel",     1, 0x0);
    oled_update();
}

static void draw_slot_select(uint8_t selected) {
    oled_clear(0xF);
    draw_string(10, 5, "select slot:", 1, 0x0);
    for (uint8_t i = 0; i < MAX_WAYPOINTS; i++) {
        uint8_t y = 22 + i * 20;
        char line[18];
        uint8_t j = 0;
        line[j++] = (i == selected) ? '>' : ' ';
        line[j++] = ' ';
        line[j++] = 's'; line[j++] = 'l'; line[j++] = 'o';
        line[j++] = 't'; line[j++] = ' ';
        line[j++] = '0' + i;
        line[j++] = ':'; line[j++] = ' ';
        const char *status = waypoint_is_valid(i) ? "saved" : "empty";
        while (*status) line[j++] = *status++;
        line[j] = '\0';
        draw_string(2, y, line, 1, 0x0);
    }
    oled_update();
}

static void draw_waypoint_list(uint8_t slot) {
    uint8_t cnt = waypoint_count();
    oled_clear(0xF);
    draw_string(20, 5, "waypoint list", 1, 0x0);
    if (cnt == 0) {
        draw_string(10, 50, "no waypoints", 1, 0x0);
    } else {
        uint8_t pos = 0;
        for (uint8_t i = 0; i <= slot; i++)
            if (waypoint_is_valid(i)) pos++;
        char line[20];
        uint8_t i = 0;
        const char *prefix = "waypoint: ";
        while (*prefix) line[i++] = *prefix++;
        line[i++] = '0' + pos;
        const char *mid = " of ";
        while (*mid) line[i++] = *mid++;
        line[i++] = '0' + cnt;
        line[i] = '\0';
        draw_string(5, 18, line, 1, 0x0);

        Waypoint wp;
        waypoint_get(slot, &wp);
        char lat_str[12], lon_str[12];
        float_to_str(wp.latitude,  lat_str);
        float_to_str(wp.longitude, lon_str);
        draw_string(2, 36, "lat:", 1, 0x0);
        draw_string(32, 36, lat_str, 1, 0x0);
        draw_string(2, 52, "lon:", 1, 0x0);
        draw_string(32, 52, lon_str, 1, 0x0);

        draw_string(10, 72, "up/dn: scroll",   1, 0x0);
        draw_string(10, 88, "right: navigate", 1, 0x0);
        draw_string(10, 104, "left: back",     1, 0x0);
    }
    oled_update();
}

// ── state machine ─────────────────────────────────────────────
// NOTE: buttons_update() is called in main.c — NOT here
void state_machine_run(uint16_t elapsed_ms) {
    static uint8_t flag_sos_rx = 0;

    switch (current_state) {

        // ── INIT ──────────────────────────────────────────────
        case STATE_INIT:
            system_init();
            menu_index      = 0;
            last_menu_index = 255;
            sos_triggered   = 0;
            current_state   = STATE_MENU;
            break;

        // ── MENU ──────────────────────────────────────────────
        case STATE_MENU:
            if (state_changed() || menu_index != last_menu_index) {
                draw_menu();
                previous_state  = current_state;
                last_menu_index = menu_index;
            }

            if (power_is_low()) { state_enter_low_power(STATE_MENU); break; }

            if (button_pressed(SOS)) {
                sos_hold_countdown_value = SOS_HOLD_COUNTDOWN_START;
                sos_hold_countdown_timer = 0;
                current_state = STATE_SOS_HOLD_COUNTDOWN;
                break;
            }

            if (button_pressed(ON_OFF)) { current_state = STATE_SHUTDOWN; break; }

            if (button_pressed(UP)) {
                menu_index = (menu_index > 0) ? menu_index - 1 : MENU_ITEM_COUNT - 1;
                break;
            }

            if (button_pressed(DOWN)) {
                menu_index = (menu_index < MENU_ITEM_COUNT - 1) ? menu_index + 1 : 0;
                break;
            }

            if (button_pressed(RIGHT)) {
                switch (menu_index) {
                    case MENU_BATTERY:
                        batt_timer    = BATT_REFRESH_MS;
                        current_state = STATE_BATTERY_CHECK;
                        break;
                    case MENU_GPS:
                        gps_view_timer = GPS_REFRESH_MS;
                        current_state  = STATE_GPS_VIEW;
                        break;
                    case MENU_WAYPOINT_SAVE:
                        wp_slot_index = 0;
                        current_state = STATE_WAYPOINT_SLOT_SELECT;
                        break;
                    case MENU_WAYPOINT_LIST:
                        waypoint_list_index = 0;
                        current_state = STATE_WAYPOINT_LIST;
                        break;
                }
                break;
            }
            break;

        // ── BATTERY CHECK ─────────────────────────────────────
        case STATE_BATTERY_CHECK:
            batt_timer += elapsed_ms;
            if (state_changed() || batt_timer >= BATT_REFRESH_MS) {
                draw_battery();
                previous_state = current_state;
                batt_timer = 0;
            }
            if (button_pressed(LEFT)) { current_state = STATE_MENU; break; }
            if (button_pressed(SOS)) {
                sos_hold_countdown_value = SOS_HOLD_COUNTDOWN_START;
                sos_hold_countdown_timer = 0;
                current_state = STATE_SOS_HOLD_COUNTDOWN;
                break;
            }
            break;

        // ── GPS VIEW ──────────────────────────────────────────
        case STATE_GPS_VIEW:
            if (state_changed()) {
                draw_gps_view();
                previous_state = current_state;
                gps_view_timer = 0;
            }
            if (!coordinates_valid) {
                gps_view_timer += elapsed_ms;
                if (gps_view_timer >= GPS_REFRESH_MS) {
                    gps_view_timer = 0;
                    draw_gps_view();
                }
            }
            if (button_pressed(LEFT)) { current_state = STATE_MENU; break; }
            if (button_pressed(SOS)) {
                sos_hold_countdown_value = SOS_HOLD_COUNTDOWN_START;
                sos_hold_countdown_timer = 0;
                current_state = STATE_SOS_HOLD_COUNTDOWN;
                break;
            }
            break;

        // ── SOS CONFIRM ───────────────────────────────────────
        case STATE_SOS_CONFIRM:
            if (state_changed()) {
                oled_clear(0xF);
                draw_string(10, 25, "are you sure?",       1, 0x0);
                draw_string(10, 50, "right: yes send sos", 1, 0x0);
                draw_string(10, 75, "left: cancel",        1, 0x0);
                oled_update();
                previous_state = current_state;
            }

            if (button_pressed(RIGHT)) {
                sos_triggered   = 1;
                countdown_value = COUNTDOWN_START;
                countdown_timer = 0;
                current_state   = STATE_SOS_COUNTDOWN;
                break;
            }

            if (button_pressed(LEFT)) {
                sos_triggered = 0;
                current_state = STATE_MENU;
                break;
            }
            break;

        // ── SOS HOLD COUNTDOWN ─────────────────────────────────
        case STATE_SOS_HOLD_COUNTDOWN:
            sos_hold_countdown_timer += elapsed_ms;

            if (state_changed()) {
                draw_sos_countdown();
                previous_state = current_state;
            }

            // Cancel if SOS button is released
            if (!button_down(SOS)) {
                current_state = STATE_MENU;
                break;
            }

            if (sos_hold_countdown_timer >= COUNTDOWN_STEP_MS) {
                sos_hold_countdown_timer = 0;
                if (sos_hold_countdown_value > 0) {
                    sos_hold_countdown_value--;
                    draw_sos_countdown();
                }

                if (sos_hold_countdown_value == 0) {
                    current_state = STATE_SOS_CONFIRM;
                    break;
                }
            }
            break;

        // ── SOS COUNTDOWN ─────────────────────────────────────
        case STATE_SOS_COUNTDOWN:
            countdown_timer += elapsed_ms;

            if (state_changed()) {
                draw_countdown();
                previous_state = current_state;
            }

            if (countdown_timer >= COUNTDOWN_STEP_MS) {
                countdown_timer = 0;
                if (countdown_value > 0) {
                    countdown_value--;
                    draw_countdown();
                }
                if (countdown_value == 0) {
                    gps_timeout   = 0;
                    current_state = STATE_GPS_CHECK;
                }
            }

            if (button_pressed(LEFT)) {
                sos_triggered = 0;
                current_state = STATE_MENU;
                break;
            }
            break;

        // ── GPS CHECK (for SOS) ───────────────────────────────
        case STATE_GPS_CHECK: {
            if (state_changed()) {
                gps_timeout    = 0;
                previous_state = current_state;
                draw_gps_check(0);

                /*
                 * Blocking GPS read:
                 * This uses the known-working gps_read() implementation.
                 * While this runs, buttons/state updates pause until an RMC sentence is read.
                 */
                coordinates_valid = gps_read(&current_gps_data);

                if (coordinates_valid && sos_triggered) {
                    gps_timeout   = 0;
                    current_state = STATE_SOS_ARMING;
                    break;
                }

                oled_clear(0xF);
                draw_string(10, 35, "gps no fix!", 1, 0x0);
                draw_string(10, 55, "try outside", 1, 0x0);
                draw_string(10, 75, "left: back",  1, 0x0);
                oled_update();
            }

            // LEFT returns after the blocking read finishes
            if (button_pressed(LEFT)) {
                gps_timeout   = 0;
                sos_triggered = 0;
                current_state = STATE_MENU;
                break;
            }
            break;
        }

        // ── SOS ARMING ────────────────────────────────────────
        case STATE_SOS_ARMING:
            if (state_changed()) {
                oled_clear(0xF);
                draw_string(10, 40, "sos armed!", 1, 0x0);
                draw_string(10, 60, "formatting...", 1, 0x0);
                oled_update();
                previous_state = current_state;
            }
            flag_sos_rx   = 0;
            current_state = STATE_SOS_FORMAT;
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
                draw_string(10, 60, "please wait",   1, 0x0);
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
                flag_sos_rx   = 0;
                current_state = STATE_SOS_FAILURE;
            }
            break;

        // ── SOS SUCCESS ───────────────────────────────────────
        case STATE_SOS_SUCCESS:
            if (state_changed()) {
                sos_triggered = 0;
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
                sos_triggered = 0;
                oled_clear(0xF);
                draw_string(10, 35, "send failed!", 1, 0x0);
                draw_string(10, 55, "sos: retry",   1, 0x0);
                draw_string(10, 75, "left: cancel", 1, 0x0);
                oled_update();
                previous_state = current_state;
            }
            if (button_pressed(SOS)) {
                sos_hold_countdown_value = SOS_HOLD_COUNTDOWN_START;
                sos_hold_countdown_timer = 0;
                current_state = STATE_SOS_HOLD_COUNTDOWN;
                break;
            }
            if (button_pressed(LEFT)) { current_state = STATE_MENU; break; }
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
            set_sleep_mode(SLEEP_MODE_PWR_SAVE);
            sleep_enable(); sei(); sleep_cpu(); sleep_disable();

            if (button_pressed(SOS)) {
                sos_hold_countdown_value = SOS_HOLD_COUNTDOWN_START;
                sos_hold_countdown_timer = 0;
                current_state = STATE_SOS_HOLD_COUNTDOWN;
                break;
            }
            if (!power_is_low()) { current_state = pre_low_power_state; break; }
            current_state = STATE_LOW_POWER;
            break;
        }

        // ── ERROR ─────────────────────────────────────────────
        case STATE_ERROR: {
            if (state_changed()) {
                oled_clear(0xF);
                draw_string(34, 30, "system",       1, 0x0);
                draw_string(40, 50, "error!",        1, 0x0);
                draw_string(10, 75, "restarting...", 1, 0x0);
                oled_update();
                previous_state = current_state;
                _delay_ms(3000);
            }
            system_init();
            sos_triggered = 0;
            current_state = STATE_MENU;
            break;
        }

        // ── SHUTDOWN ──────────────────────────────────────────
        case STATE_SHUTDOWN: {
            oled_clear(0xF);
            draw_string(28, 40, "shutting", 1, 0x0);
            draw_string(40, 60, "down...",  1, 0x0);
            oled_update();
            _delay_ms(2000);

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
            previous_state  = STATE_INIT;
            menu_index      = 0;
            last_menu_index = 255;
            sos_triggered   = 0;

            if (!(PINA & (1 << PA0))) {
                sos_triggered = 1;
                current_state = STATE_GPS_CHECK;
            } else {
                current_state = STATE_MENU;
            }
            break;
        }

        // ── WAYPOINT SLOT SELECT ──────────────────────────────
        case STATE_WAYPOINT_SLOT_SELECT:
            if (state_changed()) {
                draw_slot_select(wp_slot_index);
                previous_state = current_state;
            }
            if (button_pressed(UP)) {
                if (wp_slot_index > 0) { wp_slot_index--; draw_slot_select(wp_slot_index); }
                break;
            }
            if (button_pressed(DOWN)) {
                if (wp_slot_index < MAX_WAYPOINTS - 1) { wp_slot_index++; draw_slot_select(wp_slot_index); }
                break;
            }
            if (button_pressed(RIGHT)) {
                if (waypoint_is_valid(wp_slot_index))
                    current_state = STATE_WAYPOINT_OVERWRITE_CONFIRM;
                else
                    current_state = STATE_WAYPOINT_SAVE;
                break;
            }
            if (button_pressed(LEFT)) { current_state = STATE_MENU; break; }
            break;

        // ── WAYPOINT OVERWRITE CONFIRM ────────────────────────
        case STATE_WAYPOINT_OVERWRITE_CONFIRM:
            if (state_changed()) {
                oled_clear(0xF);
                char slot_line[16];
                uint8_t si = 0;
                const char *sp = "slot ";
                while (*sp) slot_line[si++] = *sp++;
                slot_line[si++] = '0' + wp_slot_index;
                slot_line[si++] = ' ';
                const char *sh = "has data";
                while (*sh) slot_line[si++] = *sh++;
                slot_line[si] = '\0';
                draw_string(5,  20, slot_line,       1, 0x0);
                draw_string(20, 45, "overwrite?",    1, 0x0);
                draw_string(10, 70, "right: yes",    1, 0x0);
                draw_string(10, 87, "left: no",      1, 0x0);
                oled_update();
                previous_state = current_state;
            }
            if (button_pressed(RIGHT)) { current_state = STATE_WAYPOINT_SAVE; break; }
            if (button_pressed(LEFT))  { current_state = STATE_WAYPOINT_SLOT_SELECT; break; }
            break;

        // ── WAYPOINT SAVE ─────────────────────────────────────
        case STATE_WAYPOINT_SAVE:
            if (state_changed()) {
                oled_clear(0xF);
                draw_string(10, 5, "save waypoint", 1, 0x0);
                draw_string(10, 35, "reading gps...", 1, 0x0);
                oled_update();
                wp_save_valid = gps_read(&wp_save_gps);
                oled_clear(0xF);
                draw_string(10, 5, "save waypoint", 1, 0x0);
                if (wp_save_valid) {
                    char lat_str[12], lon_str[12];
                    float_to_str(wp_save_gps.latitude,  lat_str);
                    float_to_str(wp_save_gps.longitude, lon_str);
                    draw_string(2, 28, "lat:", 1, 0x0);
                    draw_string(38, 28, lat_str, 1, 0x0);
                    draw_string(2, 43, "lon:", 1, 0x0);
                    draw_string(38, 43, lon_str, 1, 0x0);
                    draw_string(10, 65, "right: confirm", 1, 0x0);
                    draw_string(10, 80, "left: cancel",   1, 0x0);
                } else {
                    draw_string(10, 40, "no gps fix", 1, 0x0);
                    draw_string(10, 65, "left: cancel",  1, 0x0);
                }
                oled_update();
                previous_state = current_state;
            }
            if (wp_save_valid && button_pressed(RIGHT)) {
                waypoint_save_to(wp_slot_index, wp_save_gps.latitude, wp_save_gps.longitude);
                current_state = STATE_WAYPOINT_SAVED;
                break;
            }
            if (button_pressed(LEFT)) { current_state = STATE_MENU; break; }
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
                // start at first valid slot
                waypoint_list_index = 0;
                for (uint8_t i = 0; i < MAX_WAYPOINTS; i++) {
                    if (waypoint_is_valid(i)) { waypoint_list_index = i; break; }
                }
                draw_waypoint_list(waypoint_list_index);
                previous_state = current_state;
            }
            if (button_pressed(UP)) {
                for (int8_t i = waypoint_list_index - 1; i >= 0; i--) {
                    if (waypoint_is_valid(i)) {
                        waypoint_list_index = i;
                        draw_waypoint_list(waypoint_list_index);
                        break;
                    }
                }
                break;
            }
            if (button_pressed(DOWN)) {
                for (uint8_t i = waypoint_list_index + 1; i < MAX_WAYPOINTS; i++) {
                    if (waypoint_is_valid(i)) {
                        waypoint_list_index = i;
                        draw_waypoint_list(waypoint_list_index);
                        break;
                    }
                }
                break;
            }
            if (button_pressed(RIGHT)) {
                if (waypoint_is_valid(waypoint_list_index)) {
                    nav_wp_index  = waypoint_list_index;
                    nav_timer     = NAV_REFRESH_MS;
                    current_state = STATE_WAYPOINT_NAV;
                }
                break;
            }
            if (button_pressed(LEFT))  { current_state = STATE_MENU; break; }
            break;

        // ── WAYPOINT NAV ──────────────────────────────────────
        case STATE_WAYPOINT_NAV:
            if (state_changed()) {
                previous_state = current_state;
            }
            nav_timer += elapsed_ms;
            if (nav_timer >= NAV_REFRESH_MS) {
                nav_timer = 0;
                GPS_Data nav_gps;
                uint8_t nav_valid = gps_read(&nav_gps);
                oled_clear(0xF);
                char idx_str[3] = {'0' + nav_wp_index, '\0'};
                draw_string(2, 5, "waypoint: ", 1, 0x0);
                draw_string(62, 5, idx_str, 1, 0x0);
                if (nav_valid) {
                    Waypoint wp;
                    waypoint_get(nav_wp_index, &wp);
                    uint32_t dist_m;
                    uint16_t bearing;
                    const char *cardinal;
                    waypoint_navigate(nav_gps.latitude, nav_gps.longitude,
                                      wp.latitude, wp.longitude,
                                      &dist_m, &bearing, &cardinal);
                    if (dist_m < 30) {
                        draw_string(20, 50, "arrived!", 1, 0x0);
                    } else {
                        // bearing string: "315 deg nw"
                        char bear_str[12];
                        uint8_t bi = 0;
                        uint16_t b = bearing;
                        if (b >= 100) bear_str[bi++] = '0' + b/100;
                        if (b >= 10)  bear_str[bi++] = '0' + (b%100)/10;
                        bear_str[bi++] = '0' + b%10;
                        bear_str[bi++] = ' '; bear_str[bi++] = 'd';
                        bear_str[bi++] = 'e'; bear_str[bi++] = 'g';
                        bear_str[bi++] = ' ';
                        const char *cp = cardinal;
                        while (*cp) bear_str[bi++] = *cp++;
                        bear_str[bi] = '\0';
                        draw_string(2, 28, bear_str, 1, 0x0);

                        // distance string
                        char dist_str[12];
                        uint8_t di = 0;
                        if (dist_m < 200) {
                            // show in metres
                            uint16_t dm = (uint16_t)dist_m;
                            if (dm >= 100) dist_str[di++] = '0' + dm/100;
                            if (dm >= 10)  dist_str[di++] = '0' + (dm%100)/10;
                            dist_str[di++] = '0' + dm%10;
                            dist_str[di++] = ' '; dist_str[di++] = 'm';
                        } else {
                            // show in km with 1 decimal
                            uint16_t km_int = (uint16_t)(dist_m / 1000);
                            uint8_t  km_dec = (uint8_t)((dist_m % 1000) / 100);
                            if (km_int >= 100) dist_str[di++] = '0' + km_int/100;
                            if (km_int >= 10)  dist_str[di++] = '0' + (km_int%100)/10;
                            dist_str[di++] = '0' + km_int%10;
                            dist_str[di++] = '.';
                            dist_str[di++] = '0' + km_dec;
                            dist_str[di++] = ' '; dist_str[di++] = 'k'; dist_str[di++] = 'm';
                        }
                        dist_str[di] = '\0';
                        draw_string(2, 46, dist_str, 1, 0x0);
                    }
                } else {
                    draw_string(2, 40, "no gps fix", 1, 0x0);
                }
                draw_string(2, 100, "left: back", 1, 0x0);
                oled_update();
            }
            if (button_pressed(LEFT)) { current_state = STATE_WAYPOINT_LIST; break; }
            break;
    }
}
