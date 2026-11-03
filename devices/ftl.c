#include "config.h"
#include <stdint.h>
#include <stdbool.h>
#include "pio_manager.h"
#include "device.h"
#include "dac.h"
#include "hardware/clocks.h"
#include "dac.pio.h"
#include "passthrough.pio.h"

// Variables for PIO - sound and detection programs
static PIO sound_pio;
static int8_t sound_sm;
static int sound_offset;

static PIO detection_pio;
static int8_t detection_sm;
static int detection_offset;

bool load_ftl(void) {

    // Load PIO programs
    sound_offset = pio_manager_load(&sound_pio, &sound_sm, &dac_program);
    if (sound_offset < 0) {
        return false;
    }

    detection_offset = pio_manager_load(&detection_pio, &detection_sm, &passthrough_program);
    if (detection_offset < 0) {
        return false;
    }

    // Configure sound PIO GPIO pins
    pio_sm_config used_config_sound = dac_program_get_default_config(sound_offset);
    sm_config_set_in_pins(&used_config_sound, LPT_BASE_PIN);
    sm_config_set_fifo_join(&used_config_sound, PIO_FIFO_JOIN_RX);
    sm_config_set_clkdiv(&used_config_sound, (((float) clock_get_hz(clk_sys)) / (SAMPLE_RATE * 6))); // Frequency same as sample rate

    for (int i = LPT_BASE_PIN; i < LPT_BASE_PIN + 8; i++) { // Set all data pins
        pio_gpio_init(sound_pio, i);
    }

    pio_sm_set_consecutive_pindirs(sound_pio, sound_sm, LPT_BASE_PIN, 8, false); // Set pins in PIO to be inputs

    // Start sound PIO program
    if (pio_sm_init(sound_pio, sound_sm, sound_offset, &used_config_sound) < 0) {
        return false;
    }

    pio_sm_set_enabled(sound_pio, sound_sm, true);

    // Configure detection PIO GPIO pins
    pio_sm_config used_config_detection = passthrough_program_get_default_config(detection_offset);
    sm_config_set_in_pins(&used_config_detection, LPT_SELIN_PIN);
    sm_config_set_out_pins(&used_config_detection, LPT_PAPEREND_PIN, 1);
    sm_config_set_clkdiv(&used_config_detection, 10.0);

    pio_gpio_init(detection_pio, LPT_PAPEREND_PIN);
    pio_gpio_init(detection_pio, LPT_SELIN_PIN);
    pio_sm_set_consecutive_pindirs(detection_pio, detection_sm, LPT_SELIN_PIN, 1, false);
    pio_sm_set_consecutive_pindirs(detection_pio, detection_sm, LPT_PAPEREND_PIN, 1, true);

    // Start detection PIO program
    if (pio_sm_init(detection_pio, detection_sm, detection_offset, &used_config_detection) < 0) {
        return false;
    }

    pio_sm_set_enabled(detection_pio, detection_sm, true);
    return true;
}

bool unload_ftl(void) {

    // Stop PIO programs
    pio_sm_set_enabled(sound_pio, sound_sm, false);
    pio_sm_set_enabled(detection_pio, detection_sm, false);
    pio_manager_unload(sound_pio, sound_sm, sound_offset, &dac_program);
    pio_manager_unload(detection_pio, detection_sm, detection_offset, &passthrough_program);

    // Deinit all GPIO pins
    for (int i = LPT_BASE_PIN; i < LPT_BASE_PIN + 8; i++) {
        gpio_deinit(i);
    }

    gpio_deinit(LPT_PAPEREND_PIN);
    gpio_deinit(LPT_SELIN_PIN);
    return true;
}

void generate_ftl(int16_t *left_sample, int16_t *right_sample) {
    dac_generate_sample(sound_pio, sound_sm, left_sample, right_sample);
}

device_t *create_ftl(void) {
    static device_t ftl_struct = {load_ftl, unload_ftl, generate_ftl};
    return &ftl_struct;
}
