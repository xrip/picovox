#include "config.h"
#include <stdint.h>
#include <stdbool.h>
#include "pio_manager.h"
#include "device.h"
#include "dac.h"
#include "hardware/clocks.h"
#include "dac.pio.h"

// Variables for PIO
static PIO used_pio;
static int8_t used_sm;
static int used_offset;

bool load_covox(void) {

    // Load PIO program
    used_offset = pio_manager_load(&used_pio, &used_sm, &dac_program);

    if (used_offset < 0) {
        return false;
    }

    // Configure PIO GPIO pins
    pio_sm_config used_config = dac_program_get_default_config(used_offset);
    sm_config_set_in_pins(&used_config, LPT_BASE_PIN);
    sm_config_set_fifo_join(&used_config, PIO_FIFO_JOIN_RX);
    sm_config_set_clkdiv(&used_config, (((float) clock_get_hz(clk_sys)) / (SAMPLE_RATE * 6))); // Frequency same as sample rate

    for (int i = LPT_BASE_PIN; i < LPT_BASE_PIN + 8; i++) { // Set all data pins
        pio_gpio_init(used_pio, i);
    }

    pio_sm_set_consecutive_pindirs(used_pio, used_sm, LPT_BASE_PIN, 8, false); // Set pins in PIO to be inputs

    // Start PIO program
    if (pio_sm_init(used_pio, used_sm, used_offset, &used_config) < 0) {
        return false;
    }

    pio_sm_set_enabled(used_pio, used_sm, true);
    return true;
}

bool unload_covox(void) {

    // Stop PIO program
    pio_sm_set_enabled(used_pio, used_sm, false);
    pio_manager_unload(used_pio, used_sm, used_offset, &dac_program);

    // Deinit all GPIO pins
    for (int i = LPT_BASE_PIN; i < LPT_BASE_PIN + 8; i++) {
        gpio_deinit(i);
    }

    return true;
}

void generate_covox(int16_t *left_sample, int16_t *right_sample) {
    dac_generate_sample(used_pio, used_sm, left_sample, right_sample);
}

device_t *create_covox(void) {
    static device_t covox_struct = {load_covox, unload_covox, generate_covox};
    return &covox_struct;
}
