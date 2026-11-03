#ifndef DEVICE_H
#define DEVICE_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Ids of all simulated devices.
 *
 * The order is fixed by the PCVX.COM mode switching protocol, do not change it.
 */
enum device_id {
    DEVICE_COVOX,   // 0
    DEVICE_STEREO,  // 1
    DEVICE_FTL,     // 2
    DEVICE_DSS,     // 3
    DEVICE_OPL2,    // 4
    DEVICE_TANDY,   // 5
    DEVICE_CMS,     // 6
    DEVICE_OPL3,    // 7
    DEVICE_COUNT
};

/**
 * @brief Common interface for all the simulated devices.
 */
typedef struct device_t {

    /**
     * @brief Loads all the things needed for using the device (such as start PIO program, initialize registers etc.)
     *
     * @return true if device is loaded, false if anything failed.
     */
    bool (*load_device)(void);

    /**
     * @brief Unloads all the things needed after using the device (such as the PIO program etc.)
     *
     * @return true if device is unloaded, false if anything failed.
     */
    bool (*unload_device)(void);

    /**
     * @brief Reads data from PIO buffer and generates one sound sample for both channels.
     *
     * Blocks until a sample is available (samples are generated continuously, so it cannot lock).
     *
     * @param left_sample is a pointer to number where sample for left channel is placed.
     * @param right_sample is a pointer to number where sample for right channel is placed.
     */
    void (*generate_sample)(int16_t *left_sample, int16_t *right_sample);
} device_t;

/**
 * @brief Declarations for creating instances of all types of modes.
 *
 * Each mode is stored in its own file, implementing all the functions of struct above.
 * Each returns a pointer to a static instance and never fails.
 */
device_t *create_covox(void);
device_t *create_stereo(void);
device_t *create_ftl(void);
device_t *create_dss(void);
device_t *create_opl2(void);
device_t *create_tandy(void);
device_t *create_cms(void);
device_t *create_opl3(void);

#endif // DEVICE_H
