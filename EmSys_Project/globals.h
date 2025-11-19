#ifndef GLOBALS_H_
#define GLOBALS_H_

#define F_CPU 16000000UL // cpu freq
#define MAX_POINTS 25

// FLAGS & VARIABLES
bool is_system_on = false;
extern bool is_adc_on;

volatile bool start_button_pressed = false;
volatile bool left_button_pressed = false;
volatile bool right_button_pressed = false;
volatile bool touch_pending = false;

bool is_cursor_on = false;
bool is_plot_on = false;
bool is_digital_line = false;
bool is_holding = false;

bool uart_command_pending = false;

bool voltage_button_pressed = false;
bool is_high_voltage = false;

extern uint16_t display_width;
extern uint16_t display_height;

float cursor_voltage = 0;

float voltage_value;
float max_value = 0;
float min_value = 5;
float plot_points[25] = {0};
int index = 0;

bool is_calibrated = false;

#define START_BUTTON_PIN         4 // PE4
#define VOLTAGE_TYPE_TOGGLE_PIN  3 // PD3
#define CURSOR_LEFT_PIN          0 // PD0
#define CURSOR_RIGHT_PIN         1 // PD1

#define ISC0                     0
#define ISC1                     1

#define EXT_INT4                 4
#define EXT_INT3                 3
#define EXT_INT1                 1
#define EXT_INT0                 0

#endif /* GLOBALS_H_ */