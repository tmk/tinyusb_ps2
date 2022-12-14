#include <stdio.h>
#include "pico/stdlib.h"
//#include "pico/critical_section.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "hardware/irq.h"

#include "bsp/board.h"
#include "tusb.h"
#include "usb_descriptors.h"

#include "ringbuf.h"




/*
 * PS/2(IBMPC/AT Code Set 2) keyboard converter
 *
 * License: MIT
 * Copyright 2022 Jun WAKO <wakojun@gmail.com>
 *
 */
#define xprintf(s, ...)         printf(s, ##__VA_ARGS__)

// input pins: 2:clock(IRQ), 3:data
#define CLOCK_PIN   2
#define DATA_PIN    3

#define PS2_ERR_NONE    0

int ps2_error = PS2_ERR_NONE;

#define BUF_SIZE 16
static uint8_t buf[BUF_SIZE];
static ringbuf_t rbuf = {
    .buffer = buf,
    .head = 0,
    .tail = 0,
    .size_mask = BUF_SIZE - 1
};

void gpio_callback(uint gpio, uint32_t events) {
    static enum {
        INIT,
        START,
        BIT0, BIT1, BIT2, BIT3, BIT4, BIT5, BIT6, BIT7,
        PARITY,
        STOP,
    } state = INIT;
    static uint8_t data = 0;
    static uint8_t parity = 1;

    // process at falling edge of clock
    if (gpio != CLOCK_PIN) { return; }
    if (events != GPIO_IRQ_EDGE_FALL) { return; }

    //gpio_get(DATA_PIN)
    state++;
    switch (state) {
        case START:
            // start bit is low
            if (gpio_get(DATA_PIN))
                goto ERROR;
            break;
        case BIT0:
        case BIT1:
        case BIT2:
        case BIT3:
        case BIT4:
        case BIT5:
        case BIT6:
        case BIT7:
            data >>= 1;
            if (gpio_get(DATA_PIN)) {
                data |= 0x80;
                parity++;
            }
            break;
        case PARITY:
            if (gpio_get(DATA_PIN)) {
                if (!(parity & 0x01))
                    goto ERROR;
            } else {
                if (parity & 0x01)
                    goto ERROR;
            }
            break;
        case STOP:
            // stop bit is high
            if (!gpio_get(DATA_PIN))
                goto ERROR;
            // critical section for ringbuffer - need to do nothing here
            // because this should be called in IRQ context.
            // Use protection in main thread when using ringuf.
            ringbuf_put(&rbuf, data);
            goto DONE;
            break;
        default:
            goto ERROR;
    }
    return;
ERROR:
    ps2_error = state;
DONE:
    state = INIT;
    data = 0;
    parity = 1;
}

// Code Set 2 -> HID(Usage page << 12 | Usage ID)
// https://github.com/tmk/tmk_keyboard/wiki/IBM-PC-AT-Keyboard-Protocol#translation-to-hid-usages-of-microsoft
const uint16_t cs2_to_hid[] = {
    //   0       1       2       3       4       5       6       7       8       9       A       B       C       D       E       F
    0x0000, 0x0042, 0x0000, 0x003E, 0x003C, 0x003A, 0x003B, 0x0045, 0x0068, 0x0043, 0x0041, 0x003F, 0x003D, 0x002B, 0x0035, 0x0067, // 0
    0x0069, 0x00E2, 0x00E1, 0x0088, 0x00E0, 0x0014, 0x001E, 0x0000, 0x006A, 0x0000, 0x001D, 0x0016, 0x0004, 0x001A, 0x001F, 0x0000, // 1
    0x006B, 0x0006, 0x001B, 0x0007, 0x0008, 0x0021, 0x0020, 0x008C, 0x006C, 0x002C, 0x0019, 0x0009, 0x0017, 0x0015, 0x0022, 0x0000, // 2
    0x006D, 0x0011, 0x0005, 0x000B, 0x000A, 0x001C, 0x0023, 0x0000, 0x006E, 0x0000, 0x0010, 0x000D, 0x0018, 0x0024, 0x0025, 0x0000, // 3
    0x006F, 0x0036, 0x000E, 0x000C, 0x0012, 0x0027, 0x0026, 0x0000, 0x0070, 0x0037, 0x0038, 0x000F, 0x0033, 0x0013, 0x002D, 0x0000, // 4
    0x0071, 0x0087, 0x0034, 0x0000, 0x002F, 0x002E, 0x0000, 0x0072, 0x0039, 0x00E5, 0x0028, 0x0030, 0x0000, 0x0031, 0x0000, 0x0073, // 5
    0x0000, 0x0064, 0x0093, 0x0092, 0x008A, 0x0000, 0x002A, 0x008B, 0x0000, 0x0059, 0x0089, 0x005C, 0x005F, 0x0085, 0x0000, 0x0000, // 6
    0x0062, 0x0063, 0x005A, 0x005D, 0x005E, 0x0060, 0x0029, 0x0053, 0x0044, 0x0057, 0x005B, 0x0056, 0x0055, 0x0061, 0x0047, 0x0046, // 7
    0x0000, 0x0000, 0x0000, 0x0040, 0x0046, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, // 8
    0xC221, 0x00E6, 0x0000, 0x0000, 0x00E4, 0xC0B6, 0x0000, 0x0000, 0xC22A, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x00E3, // 9
    0xC227, 0xC0EA, 0x0000, 0xC0E2, 0x0000, 0x0000, 0x0000, 0x00E7, 0xC226, 0x0000, 0x0000, 0xC192, 0x0000, 0x0000, 0x0000, 0x0065, // A
    0xC225, 0x0000, 0xC0E9, 0x0000, 0xC0CD, 0x0000, 0x0000, 0x1081, 0xC224, 0x0000, 0xC223, 0xC0B7, 0x0000, 0x0000, 0x0000, 0x1082, // B
    0xC194, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xC18A, 0x0000, 0x0054, 0x0000, 0x0000, 0xC0B5, 0x0000, 0x0000, // C
    0xC183, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0058, 0x0000, 0x0000, 0x0000, 0x1083, 0x0000, // D
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x004D, 0x0000, 0x0050, 0x004A, 0x0000, 0x0000, 0x0000, // E
    0x0049, 0x004C, 0x0051, 0x0000, 0x004F, 0x0052, 0x0000, 0x0048, 0x0000, 0x0000, 0x004E, 0x0000, 0x0046, 0x004B, 0x0048, 0x0000, // F
};

enum CS2_state {
    CS2_INIT,
    CS2_F0,
    CS2_E0,
    CS2_E0_F0,
    // Pause
    CS2_E1,
    CS2_E1_14,
    CS2_E1_F0,
    CS2_E1_F0_14,
    CS2_E1_F0_14_F0,
} state_cs2 = CS2_INIT;

void make_code(uint16_t code);
void break_code(uint16_t code);

// from TMK ibmpc_usb converter
int8_t process_cs2(uint8_t code)
{
    switch (state_cs2) {
        case CS2_INIT:
            switch (code) {
                case 0xE0:
                    state_cs2 = CS2_E0;
                    break;
                case 0xF0:
                    state_cs2 = CS2_F0;
                    break;
                case 0xE1:
                    state_cs2 = CS2_E1;
                    break;
                case 0x00 ... 0x7F:
                case 0x83:  // F7
                case 0x84:  // Alt'd PrintScreen
                    make_code(cs2_to_hid[code]);
                    break;
                case 0xAA:  // Self-test passed
                case 0xFC:  // Self-test failed
                case 0xF1:  // Korean Hanja          - not support
                case 0xF2:  // Korean Hangul/English - not support
                    break;
                default:    // unknown codes
                    xprintf("!CS2_INIT!\n");
                    return -1;
            }
            break;
        case CS2_E0:    // E0-Prefixed
            switch (code) {
                case 0x12:  // to be ignored
                case 0x59:  // to be ignored
                    state_cs2 = CS2_INIT;
                    break;
                case 0xF0:
                    state_cs2 = CS2_E0_F0;
                    break;
                default:
                    state_cs2 = CS2_INIT;
                    if (code < 0x80) {
                        make_code(cs2_to_hid[code | 0x80]);
                    } else {
                        xprintf("!CS2_E0!\n");
                        return -1;
                    }
            }
            break;
        case CS2_F0:    // Break code
            switch (code) {
                case 0x00 ... 0x7F:
                case 0x83:  // F7
                case 0x84:  // Alt'd PrintScreen
                    state_cs2 = CS2_INIT;
                    break_code(cs2_to_hid[code]);
                    break;
                default:
                    state_cs2 = CS2_INIT;
                    xprintf("!CS2_F0! %02X\n", code);
                    return -1;
            }
            break;
        case CS2_E0_F0: // Break code of E0-prefixed
            switch (code) {
                case 0x12:  // to be ignored
                case 0x59:  // to be ignored
                    state_cs2 = CS2_INIT;
                    break;
                default:
                    state_cs2 = CS2_INIT;
                    if (code < 0x80) {
                        break_code(cs2_to_hid[code | 0x80]);
                    } else {
                        xprintf("!CS2_E0_F0!\n");
                        return -1;
                    }
            }
            break;
        // Pause make: E1 14 77
        case CS2_E1:
            switch (code) {
                case 0x14:
                    state_cs2 = CS2_E1_14;
                    break;
                case 0xF0:
                    state_cs2 = CS2_E1_F0;
                    break;
                default:
                    state_cs2 = CS2_INIT;
            }
            break;
        case CS2_E1_14:
            switch (code) {
                case 0x77:
                    make_code(cs2_to_hid[code | 0x80]);
                    state_cs2 = CS2_INIT;
                    break;
                default:
                    state_cs2 = CS2_INIT;
            }
            break;
        // Pause break: E1 F0 14 F0 77
        case CS2_E1_F0:
            switch (code) {
                case 0x14:
                    state_cs2 = CS2_E1_F0_14;
                    break;
                default:
                    state_cs2 = CS2_INIT;
            }
            break;
        case CS2_E1_F0_14:
            switch (code) {
                case 0xF0:
                    state_cs2 = CS2_E1_F0_14_F0;
                    break;
                default:
                    state_cs2 = CS2_INIT;
            }
            break;
        case CS2_E1_F0_14_F0:
            switch (code) {
                case 0x77:
                    break_code(cs2_to_hid[code | 0x80]);
                    state_cs2 = CS2_INIT;
                    break;
                default:
                    state_cs2 = CS2_INIT;
            }
            break;
        default:
            state_cs2 = CS2_INIT;
    }
    return 0;
}








// imported from tinyusb/examples/device/hid_composite
/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */
void led_blinking_task(void);
void hid_task(void);

int main() {
    board_init();
    tud_init(BOARD_TUD_RHPORT);
    stdio_init_all();

    //critical_section_t crit_rbuf;
    //critical_section_init(&crit_rbuf);

    // input pins: clock(IRQ at falling edge)
    gpio_init(CLOCK_PIN);
    gpio_set_irq_enabled_with_callback(CLOCK_PIN, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    gpio_init(DATA_PIN);
    gpio_set_dir(DATA_PIN, GPIO_IN);

    printf("\ntinyusb_ps2\n");
    while (true) {
        //irq_set_enabled(IO_IRQ_BANK0, false);             // disable only GPIO IRQ
        //critical_section_enter_blocking(&crit_rbuf);      // disable IRQ and spin_lock
        uint32_t status = save_and_disable_interrupts();    // disable IRQ
        int c = ringbuf_get(&rbuf); // critical_section
        restore_interrupts(status);
        //critical_section_exit(&crit_rbuf);
        //irq_set_enabled(IO_IRQ_BANK0, true);

        if (c != -1) {
            printf("%02X ", c);
            process_cs2((uint8_t) c);
        }

        tud_task();
        hid_task();
        led_blinking_task();
    }
    return 0;
}



//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
/* Blink pattern
 * - 250 ms  : device not mounted
 * - 1000 ms : device mounted
 * - 2500 ms : device is suspended
 */
enum  {
  BLINK_NOT_MOUNTED = 250,
  BLINK_MOUNTED = 1000,
  BLINK_SUSPENDED = 2500,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;
void led_blinking_task(void)
{
  static uint32_t start_ms = 0;
  static bool led_state = false;

  // blink is disabled
  if (!blink_interval_ms) return;

  // Blink every interval ms
  if ( board_millis() - start_ms < blink_interval_ms) return; // not enough time
  start_ms += blink_interval_ms;

  board_led_write(led_state);
  led_state = 1 - led_state; // toggle
  //printf("Blink!\n");
}




//--------------------------------------------------------------------+
// USB HID
//--------------------------------------------------------------------+
static hid_keyboard_report_t keyboard_report;

void keyboard_add_mod(uint8_t key)
{
    keyboard_report.modifier |= (uint8_t) (1 << (key & 0x7));
}

void keyboard_del_mod(uint8_t key)
{
    keyboard_report.modifier &= (uint8_t) ~(1 << (key & 0x7));
}

void keyboard_add_key(uint8_t key)
{
    int empty = -1;
    for (int i = 0; i < 6; i++) {
        if (keyboard_report.keycode[i] == key) {
            return;
        }
        if (empty == -1 && keyboard_report.keycode[i] == 0) {
            empty = i;
        }
    }
    if (empty != -1) {
        keyboard_report.keycode[empty] = key;
    }
}

void keyboard_del_key(uint8_t key)
{
    for (int i = 0; i < 6; i++) {
        if (keyboard_report.keycode[i] == key) {
            keyboard_report.keycode[i] = 0;
            return;
        }
    }
}

void print_report(void)
{
    printf("\n");
    uint8_t *p = (uint8_t *) &keyboard_report;
    for (uint i = 0; i < sizeof(keyboard_report); i++) {
        printf("%02X ", *p++);
    }
    printf("\n");
}

void make_code(uint16_t code)
{
    // usage page
    uint8_t page = (uint8_t) ((code & 0xf000) >> 12);
    switch (page) {
        case 0x0:
        case 0x7: // keyboard
            {
                uint8_t c = (uint8_t) (code & 0xFF);
                if (c >= 0xE0 && c <= 0xE8) {
                    keyboard_add_mod(c);
                } else {
                    keyboard_add_key(c);
                }
            }
            break;
        case 0x1: // system
        case 0xC: // consumer
        default:
            break;
    }
    print_report();
}

void break_code(uint16_t code)
{
    // usage page
    uint8_t page = (uint8_t) ((code & 0xf000) >> 12);
    switch (page) {
        case 0x0:
        case 0x7: // Keyboard
            {
                uint8_t c = (uint8_t) (code & 0xFF);
                if (c >= 0xE0 && c <= 0xE8) {
                    keyboard_del_mod(c);
                } else {
                    keyboard_del_key(c);
                }
            }
            break;
        case 0x1: // System
        case 0xC: // Consumer
        default:
            break;
    }
    print_report();
}

static void send_hid_report(uint8_t report_id, uint32_t btn)
{
  // skip if hid is not ready yet
  if ( !tud_hid_ready() ) return;

  switch(report_id)
  {
    case REPORT_ID_KEYBOARD:
    {
      // use to avoid send multiple consecutive zero report for keyboard
      static bool has_keyboard_key = false;

      if ( btn )
      {
        uint8_t keycode[6] = { 0 };
        keycode[0] = HID_KEY_A;

        tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, keycode);
        has_keyboard_key = true;
      }else
      {
        // send empty key report if previously has key pressed
        if (has_keyboard_key) tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, NULL);
        has_keyboard_key = false;
      }
    }
    break;

    case REPORT_ID_MOUSE:
    {
      int8_t const delta = 5;

      // no button, right + down, no scroll, no pan
      tud_hid_mouse_report(REPORT_ID_MOUSE, 0x00, delta, delta, 0, 0);
    }
    break;

    case REPORT_ID_CONSUMER_CONTROL:
    {
      // use to avoid send multiple consecutive zero report
      static bool has_consumer_key = false;

      if ( btn )
      {
        // volume down
        uint16_t volume_down = HID_USAGE_CONSUMER_VOLUME_DECREMENT;
        tud_hid_report(REPORT_ID_CONSUMER_CONTROL, &volume_down, 2);
        has_consumer_key = true;
      }else
      {
        // send empty key report (release key) if previously has key pressed
        uint16_t empty_key = 0;
        if (has_consumer_key) tud_hid_report(REPORT_ID_CONSUMER_CONTROL, &empty_key, 2);
        has_consumer_key = false;
      }
    }
    break;

    case REPORT_ID_GAMEPAD:
    {
      // use to avoid send multiple consecutive zero report for keyboard
      static bool has_gamepad_key = false;

      hid_gamepad_report_t report =
      {
        .x   = 0, .y = 0, .z = 0, .rz = 0, .rx = 0, .ry = 0,
        .hat = 0, .buttons = 0
      };

      if ( btn )
      {
        report.hat = GAMEPAD_HAT_UP;
        report.buttons = GAMEPAD_BUTTON_A;
        tud_hid_report(REPORT_ID_GAMEPAD, &report, sizeof(report));

        has_gamepad_key = true;
      }else
      {
        report.hat = GAMEPAD_HAT_CENTERED;
        report.buttons = 0;
        if (has_gamepad_key) tud_hid_report(REPORT_ID_GAMEPAD, &report, sizeof(report));
        has_gamepad_key = false;
      }
    }
    break;

    default: break;
  }
}

// Every 10ms, we will sent 1 report for each HID profile (keyboard, mouse etc ..)
// tud_hid_report_complete_cb() is used to send the next report after previous one is complete
void hid_task(void)
{
  // Poll every 10ms
  const uint32_t interval_ms = 10;
  static uint32_t start_ms = 0;

  if ( board_millis() - start_ms < interval_ms) return; // not enough time
  start_ms += interval_ms;

  //uint32_t const btn = board_button_read();
  uint32_t const btn = 0;

  // Remote wakeup
  if ( tud_suspended() && btn )
  {
    // Wake up host if we are in suspend mode
    // and REMOTE_WAKEUP feature is enabled by host
    tud_remote_wakeup();
  }else
  {
    // Send the 1st of report chain, the rest will be sent by tud_hid_report_complete_cb()
    send_hid_report(REPORT_ID_KEYBOARD, btn);
  }
}

// Invoked when sent REPORT successfully to host
// Application can use this to send the next report
// Note: For composite reports, report[0] is report ID
void tud_hid_report_complete_cb(uint8_t instance, uint8_t const* report, uint8_t len)
{
  (void) instance;
  (void) len;

  uint8_t next_report_id = report[0] + 1;

  if (next_report_id < REPORT_ID_COUNT)
  {
    send_hid_report(next_report_id, board_button_read());
  }
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
  // TODO not Implemented
  (void) instance;
  (void) report_id;
  (void) report_type;
  (void) buffer;
  (void) reqlen;

  return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
  (void) instance;

  if (report_type == HID_REPORT_TYPE_OUTPUT)
  {
    // Set keyboard LED e.g Capslock, Numlock etc...
    if (report_id == REPORT_ID_KEYBOARD)
    {
      // bufsize should be (at least) 1
      if ( bufsize < 1 ) return;

      uint8_t const kbd_leds = buffer[0];

      if (kbd_leds & KEYBOARD_LED_CAPSLOCK)
      {
        // Capslock On: disable blink, turn led on
        blink_interval_ms = 0;
        board_led_write(true);
      }else
      {
        // Caplocks Off: back to normal blink
        board_led_write(false);
        blink_interval_ms = BLINK_MOUNTED;
      }
    }
  }
}
