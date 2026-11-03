#include "config.h"
#include <stdint.h>
#include <stdbool.h>
#include "pio_manager.h"
#include "ringbuffer.h"
#include "device.h"
#include "square/square_c.h"
#include "pico/multicore.h"
#include "cms.pio.h"

// Buffer storing samples generated
#define CMS_RINGBUFFER_SIZE 2048

// Sample repeated 2 times -> for 96kHz, only 48 kHz needed (still high quality, but fast enough)
#define SAMPLE_REPEAT 2

// Variables for PIO - each chip has its own
static PIO first_pio;
static int8_t first_sm;
static int first_offset;

static PIO second_pio;
static int8_t second_sm;
static int second_offset;

// Since emulator runs at half the rate, repeat all samples twice
static int8_t sample_used = 0;

// Killswitch for core 1
static volatile bool stop_core1 = false;

// Chip emulation - running on core 1
static void write_to_chip(gameblaster_t *device, int16_t current_instruction, bool first) {
    uint32_t address = 0x220; // Address as in CMS -> determines chip number and address/data selection

    // Determines first or second chip
    if (!first) {
        address += 0x2;
    }

    // Strobe (bit 8) determines address or data write
    if (((current_instruction >> 8) & 1) == 1) {
        address += 0x1;
    }

    uint8_t data = current_instruction & 255;
    gameblaster_write(device, address, data);
}

static void load_new_instruction(gameblaster_t *device) {

    // Data for first chip
    if (!pio_sm_is_rx_fifo_empty(first_pio, first_sm)) {
        write_to_chip(device, pio_sm_get(first_pio, first_sm) >> 23, true);
    }

    // Data for second chip
    if (!pio_sm_is_rx_fifo_empty(second_pio, second_sm)) {
        write_to_chip(device, pio_sm_get(second_pio, second_sm) >> 23, false);
    }
}

static void core1_operation(void) {
    gameblaster_t *device = gameblaster_create();
    int32_t current_left_sample = 0;
    int32_t current_right_sample = 0;

    while (!stop_core1) {

        // Before generating new sample load all instructions
        while ((!pio_sm_is_rx_fifo_empty(first_pio, first_sm)) || (!pio_sm_is_rx_fifo_empty(second_pio, second_sm))) {
            load_new_instruction(device);
        }

        gameblaster_get_sample(device, &current_left_sample, &current_right_sample);

        // While ringbuffer is full, load new instructions
        while (ringbuffer_is_full() && !stop_core1) {
            load_new_instruction(device);
        }

        ringbuffer_push(current_left_sample >> 1);
        ringbuffer_push(current_right_sample >> 1);
    }

    // Core 1 should be stopped -> remove device from memory
    gameblaster_destroy(device);
}

bool load_cms(void) {
    ringbuffer_init(CMS_RINGBUFFER_SIZE);

    // Load PIO programs (each chip has its own)
    first_offset = pio_manager_load(&first_pio, &first_sm, &cms_program);

    if (first_offset < 0) {
        return false;
    }

    second_offset = pio_manager_load(&second_pio, &second_sm, &cms_program);

    if (second_offset < 0) {
        return false;
    }

    // Configure first PIO GPIO pins
    pio_sm_config first_config = cms_program_get_default_config(first_offset);
    sm_config_set_in_pins(&first_config, LPT_BASE_PIN);
    sm_config_set_fifo_join(&first_config, PIO_FIFO_JOIN_RX);
    sm_config_set_jmp_pin(&first_config, LPT_AUTOFEED_PIN);

    for (int i = LPT_BASE_PIN; i < LPT_BASE_PIN + 8; i++) { // Set all data pins
        pio_gpio_init(first_pio, i);
    }

    pio_gpio_init(first_pio, LPT_STROBE_PIN);
    pio_gpio_init(first_pio, LPT_INIT_PIN);
    pio_gpio_init(first_pio, LPT_AUTOFEED_PIN);

    pio_sm_set_consecutive_pindirs(first_pio, first_sm, LPT_BASE_PIN, 9, false); // Set pins in PIO to be inputs

    // Start first PIO program
    if (pio_sm_init(first_pio, first_sm, first_offset, &first_config) < 0) {
        return false;
    }

    pio_sm_set_enabled(first_pio, first_sm, true);

    // Configure second PIO GPIO pins
    pio_sm_config second_config = cms_program_get_default_config(second_offset);
    sm_config_set_in_pins(&second_config, LPT_BASE_PIN);
    sm_config_set_fifo_join(&second_config, PIO_FIFO_JOIN_RX);
    sm_config_set_jmp_pin(&second_config, LPT_SELIN_PIN);

    for (int i = LPT_BASE_PIN; i < LPT_BASE_PIN + 8; i++) { // Set all data pins
        pio_gpio_init(second_pio, i);
    }

    pio_gpio_init(second_pio, LPT_STROBE_PIN);
    pio_gpio_init(second_pio, LPT_INIT_PIN);
    pio_gpio_init(second_pio, LPT_SELIN_PIN);

    pio_sm_set_consecutive_pindirs(second_pio, second_sm, LPT_BASE_PIN, 9, false); // Set pins in PIO to be inputs

    // Start second PIO program
    if (pio_sm_init(second_pio, second_sm, second_offset, &second_config) < 0) {
        return false;
    }

    pio_sm_set_enabled(second_pio, second_sm, true);

    // Start core 1
    stop_core1 = false;
    multicore_reset_core1();
    multicore_launch_core1(core1_operation);
    return true;
}

bool unload_cms(void) {

    // Stop core 1
    stop_core1 = true;

    // Stop PIO programs
    pio_sm_set_enabled(first_pio, first_sm, false);
    pio_manager_unload(first_pio, first_sm, first_offset, &cms_program);

    pio_sm_set_enabled(second_pio, second_sm, false);
    pio_manager_unload(second_pio, second_sm, second_offset, &cms_program);

    // Deinit all GPIO pins
    for (int i = LPT_BASE_PIN; i < LPT_BASE_PIN + 8; i++) {
        gpio_deinit(i);
    }

    gpio_deinit(LPT_STROBE_PIN);
    gpio_deinit(LPT_AUTOFEED_PIN);
    gpio_deinit(LPT_INIT_PIN);
    gpio_deinit(LPT_SELIN_PIN);
    return true;
}

void generate_cms(int16_t *left_sample, int16_t *right_sample) {

    if (sample_used >= SAMPLE_REPEAT) {

        // Wait for sample if none is generated (cannot lock since samples are generated automatically)
        while (ringbuffer_is_empty()) {
            tight_loop_contents();
        }

        // Cannot happen, added as a failsafe
        if (!ringbuffer_pop(left_sample)) {
            *left_sample = 0;
            *right_sample = 0;
        } else {

            // If left sample is present, right must be present too - wait if none
            while (ringbuffer_is_empty()) {
                tight_loop_contents();
            }

            ringbuffer_pop(right_sample);
        }

        sample_used = 0;
    }

    sample_used++;
}

device_t *create_cms(void) {
    static device_t cms_struct = {load_cms, unload_cms, generate_cms};
    return &cms_struct;
}