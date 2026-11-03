#include "config.h"
#include <stdint.h>
#include <stdbool.h>
#include "device.h"
#include "pico/stdlib.h"
#include "pio_manager.h"
#include "mode_switch.h"
#include "mode_switch.pio.h"

// Variables for PIO
static PIO used_pio;
static int8_t used_sm;
static int used_offset;

bool init_mode_change(void) {

    // Load PIO program
    used_offset = pio_manager_load(&used_pio, &used_sm, &mode_switch_program);

    if (used_offset < 0) {
        return false;
    }

    // Configure PIO GPIO pins
    pio_sm_config used_config = mode_switch_program_get_default_config(used_offset);
    sm_config_set_in_pins(&used_config, LPT_BASE_PIN);
    sm_config_set_fifo_join(&used_config, PIO_FIFO_JOIN_RX);
    sm_config_set_clkdiv(&used_config, 100);

    for (int i = LPT_BASE_PIN; i < LPT_BASE_PIN + 12; i++) { // Set all data and control pins
        pio_gpio_init(used_pio, i);
    }

    pio_sm_set_consecutive_pindirs(used_pio, used_sm, LPT_BASE_PIN, 12, false); // Set pins in PIO to be inputs

    // Start PIO program
    if (pio_sm_init(used_pio, used_sm, used_offset, &used_config) < 0) {
        return false;
    }

    pio_sm_set_enabled(used_pio, used_sm, true);

    // Set output pins to be low
    gpio_init(LPT_ERROR_PIN);
    gpio_put(LPT_ERROR_PIN, 0);
    gpio_set_dir(LPT_ERROR_PIN, true);
    gpio_init(LPT_SELECT_PIN);
    gpio_put(LPT_SELECT_PIN, 0);
    gpio_set_dir(LPT_SELECT_PIN, true);
    gpio_init(LPT_BUSY_PIN);
    gpio_put(LPT_BUSY_PIN, 0);
    gpio_set_dir(LPT_BUSY_PIN, true);
    gpio_init(LPT_DATA_DIR);
    gpio_put(LPT_DATA_DIR, 0);
    gpio_set_dir(LPT_DATA_DIR, true);
    return true;
}

// Even parity: XOR of all 8 bits (7 command bits + lowest parity bit) must be 0
static bool check_parity(uint8_t command) {
    uint8_t bits = command;
    bits ^= bits >> 4;
    bits ^= bits >> 2;
    bits ^= bits >> 1;
    return (bits & 1) == 0;
}

static void send_number(int8_t current) {

    for (uint8_t checked_bit = 0x10; checked_bit > 0; checked_bit >>= 1) {

        if ((checked_bit & current) != 0) {
            gpio_put(LPT_SELECT_PIN, 0);
            gpio_put(LPT_ERROR_PIN, 1);
        } else {
            gpio_put(LPT_ERROR_PIN, 0);
            gpio_put(LPT_SELECT_PIN, 1);
        }

        gpio_put(LPT_BUSY_PIN, 1);
        sleep_ms(100);
        gpio_put(LPT_BUSY_PIN, 0);
        gpio_put(LPT_ERROR_PIN, 0);
        gpio_put(LPT_SELECT_PIN, 0);
        sleep_ms(100);
    }

}

void mode_change(int8_t *change_to, int8_t current, device_t ** device_list) {

    if (pio_sm_is_rx_fifo_empty(used_pio, used_sm)) {
        return;
    }

    uint8_t wanted_mode = ((pio_sm_get(used_pio, used_sm) >> 24));
    if (check_parity(wanted_mode)) {
        gpio_put(LPT_SELECT_PIN, 1);
        sleep_ms(100);
        gpio_put(LPT_SELECT_PIN, 0);
    } else {
        gpio_put(LPT_ERROR_PIN, 1);
        sleep_ms(100);
        gpio_put(LPT_ERROR_PIN, 0);
        return;
    }

    // Change emulated device
    if ((wanted_mode >> 6) == 3) {
        uint8_t wanted_device = ((wanted_mode >> 1) & 31) - 2;
        *change_to = (wanted_device < DEVICE_COUNT) ? wanted_device : 0;
        return;
    }

    // Return status
    if (wanted_mode == 170) {
        sleep_ms(50);
        if (current == DEVICE_STEREO) { // Stereo uses the BUSY pin in GPIO, not PIO
            device_list[current]->unload_device();
        }
        send_number(current);
        if (current == DEVICE_STEREO) {
            device_list[current]->load_device();
        }
        return;
    }
}