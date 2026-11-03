#ifndef CONFIG_H
#define CONFIG_H

#define SAMPLE_RATE 96000

// Definitions of GPIO LPT pins
// Beware! The PIO programs hardcode the control pin positions (absolute GPIO numbers),
// so STROBE must directly follow the DATA pins (LPT_BASE_PIN + 8).
#define LPT_BASE_PIN 1
#define LPT_STROBE_PIN 9
#define LPT_AUTOFEED_PIN 10
#define LPT_INIT_PIN 11
#define LPT_SELIN_PIN 12
#define LPT_ERROR_PIN 13
#define LPT_ACK_PIN 14
#define LPT_BUSY_PIN 15
#define LPT_PAPEREND_PIN 16
#define LPT_SELECT_PIN 17

#define LPT_DATA_DIR 0     // Pin for switching the D0-D7 direction (PC->picovox × picovox->PC)

// Definitions of GPIO I2S pins
#define PICO_AUDIO_I2S_CLOCK_PIN_BASE 27
#define PICO_AUDIO_I2S_DATA_PIN 26

// Definition of unused pin
#define PICO_UNUSED_PIN 22

// Defnition of Volume Mute button
#define PICO_AUDIO_MUTE 29

#endif // CONFIG_H
