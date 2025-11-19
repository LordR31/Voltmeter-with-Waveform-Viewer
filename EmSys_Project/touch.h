#ifndef TOUCH_H_
#define TOUCH_H_

#include "stdbool.h"
#include "spi.h"
#include "display.h"

#define TOUCH_IRQ_PIN PD2
#define GET_X_COMMAND 0x90
#define GET_Y_COMMAND 0xD0

#define BUTTON_X_START 832
#define BUTTON_X_END 384

#define CURSOR_Y_TOP 2976
#define HOLD_Y_TOP 2248
#define WAVE_Y_TOP 1728
#define TOGGLE_Y_TOP 1088
#define TOGGLE_Y_BOTTOM 544

#define CURSOR_BUTTON 1
#define HOLD_BUTTON 2
#define WAVE_BUTTON 3
#define TOGGLE_BUTTON 4
#define CALIBRATION 5

#define CALIBRATION_BUTTON_X_START 3200
#define CALIBRATOIN_BUTTON_Y_START 1450
#define CALIBRATION_BUTTON_X_END 1248
#define CALBIRATION_BUTTON_Y_END 1000

void Touch_IRQ_Init(void);
uint16_t touch_spi_transfer(uint8_t command);
uint16_t read_touch_x(void);
uint16_t read_touch_y(void);
uint8_t check_touch_buttons(uint16_t x, uint16_t y);
void execute_button_command(int which_button);
void touchscreen_process_commad();

#endif /* TOUCH_H_ */