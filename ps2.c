#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/sync.h"
#include "hardware/irq.h"

#include "bsp/board.h"
#include "tusb.h"
#include "usb_descriptors.h"

#include "ringbuf.h"

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

int main() {
    stdio_init_all();
    board_init();
    tud_init(BOARD_TUD_RHPORT);
    

    // input pins: clock(IRQ at falling edge)
    gpio_init(CLOCK_PIN);
    gpio_set_irq_enabled_with_callback(CLOCK_PIN, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    gpio_init(DATA_PIN);
    gpio_set_dir(DATA_PIN, GPIO_IN);

    printf("\nmytest ");
    while (true) {
        //__wfi();
        //__wfe();
        //printf(".");
        irq_set_enabled(IO_IRQ_BANK0, false);
        if (!ringbuf_is_empty(&rbuf)) {
            printf("%02X ", ringbuf_get(&rbuf));
        }
        irq_set_enabled(IO_IRQ_BANK0, true);
    }
    return 0;
}
