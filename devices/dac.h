#ifndef DAC_H
#define DAC_H

#include <stdint.h>
#include "hardware/pio.h"

/**
 * @brief Shared sample reading for simple DAC devices (Covox, FTL).
 *
 * The PIO program pushes the 8 data pins at the sample rate, so this blocks
 * only until the next pin snapshot arrives.
 */

// Reads one raw LPT sample from the PIO FIFO.
static inline int16_t dac_read_sample(PIO pio, uint sm) {

    // Wait for sample (generated continuously -> will not lock)
    while (pio_sm_is_rx_fifo_empty(pio, sm)) {
        tight_loop_contents();
    }

    // If all pins low -> empty sample, otherwise subtract 128 (to generate negative amplitude)
    uint8_t result = ((pio_sm_get(pio, sm) >> 24) & 0xFF);
    return (result == 0) ? 0 : (int16_t)((result - 128) << 6);
}

// Returns middle value of three to remove random wrong reads.
static inline int16_t dac_best_sample(int16_t a, int16_t b, int16_t c) {
    int16_t max_ab = (a < b) ? b : a;
    int16_t min_ab = (a < b) ? a : b;

    if (c >= max_ab) {
        return max_ab;
    }

    if (c >= min_ab) {
        return c;
    }

    return min_ab;
}

// Generates one sample (median of three reads), same on both channels.
static inline void dac_generate_sample(PIO pio, uint sm, int16_t *left_sample, int16_t *right_sample) {
    int16_t current_sample = dac_best_sample(dac_read_sample(pio, sm),
                                             dac_read_sample(pio, sm),
                                             dac_read_sample(pio, sm));
    *left_sample = current_sample;
    *right_sample = current_sample;
}

#endif // DAC_H
