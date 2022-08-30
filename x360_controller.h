#include <stdint.h>

// reports from the controller all start with this header
typedef struct _xinput_report {
    uint8_t message_type;
    uint8_t message_size;
} __attribute__((packed)) xinput_report;
// report containing controller state
typedef struct _xinput_report_controls {
    xinput_report header;
    uint8_t buttons1;
    uint8_t buttons2;
    uint8_t left_trigger;
    uint8_t right_trigger;
    int16_t left_stick_x;
    int16_t left_stick_y;
    int16_t right_stick_x;
    int16_t right_stick_y;
    uint8_t padding[6];
} __attribute__((packed)) xinput_report_controls;