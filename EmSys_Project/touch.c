#include "touch.h"

extern bool touch_pending;
extern bool is_plot_on;
extern bool is_cursor_on;
extern bool is_holding;
extern bool is_digital_line;

extern int index;
extern float min_value;
extern float max_value;
extern float plot_points[];

extern bool is_calibrated;


void Touch_IRQ_Init(void){
	DDRD &= ~(1 << TOUCH_IRQ_PIN);                       // PD2 as input
	PORTD |= (1 << TOUCH_IRQ_PIN);                       // Pull-up ON
	
	EICRA &= ~(1 << ISC20);                              // clear ISC20
	EICRA |= (1 << ISC21);                               // set ISC21 (10 falling edge)
	EIMSK |= (1 << INT2);                                // Enable INT2
}

uint16_t touch_spi_transfer(uint8_t command) {
	CS_TOUCH_LOW();                                      // set Touch Chip Select low to signal we talk to it

	SPI_transfer(command);                               // discard first response (garbage)
	uint8_t high = SPI_transfer(0x00);                   // get the high byte
	uint8_t low  = SPI_transfer(0x00);                   // get the low byte

	CS_TOUCH_HIGH();                                     // set  Touch Chip Select high to signal we stopped talking to it

	return ((high << 8) | low) >> 3;                     // return the response, shift by 3 to the right because XPT2046 returns 12 bytes, not 16 (it's a 12bit ADC)
}

uint16_t read_touch_x(void) {                            // function to get the x coord
	return touch_spi_transfer(GET_X_COMMAND);
}

uint16_t read_touch_y(void) {                            // function to get the y coord
	return touch_spi_transfer(GET_Y_COMMAND);
}

uint8_t check_touch_buttons(uint16_t x, uint16_t y){     // function to if the touch was accidental or on purpose (it was a button press) 
	if(is_calibrated){
		if((x <= BUTTON_X_START) & (x >= BUTTON_X_END))          // if we are within the limits of the button menu for X
			if((y <= CURSOR_Y_TOP) & (y > HOLD_Y_TOP))           // then, if we are within the vertical limits of the Cursor Button
				return CURSOR_BUTTON;                        // return that it was a Cursor Button press
			else if((y <= HOLD_Y_TOP) & (y > WAVE_Y_TOP))        // otherwise, if we are within the vertical limits of the Hold Button
				return HOLD_BUTTON;                          // return that it was a Hold Button press
			else if((y <= WAVE_Y_TOP) & (y > TOGGLE_Y_TOP))      // otherwise, if we are within the vertical limits of the Waveform Viewer Button
				return WAVE_BUTTON;                          // return that it was a Waveform Viewer Button press
			else if((y <= TOGGLE_Y_TOP) & (y > TOGGLE_Y_BOTTOM)) // otherwise, if we are within the vertical limits of the Toggle Waveform Type Button
				return TOGGLE_BUTTON;                        // return that it was a Toggle Waveform Type Button press
	}else{
		if((x <= CALIBRATION_BUTTON_X_START) && (x >= CALIBRATION_BUTTON_X_END) && (y <= CALIBRATOIN_BUTTON_Y_START) && (y >= CALBIRATION_BUTTON_Y_END)){
			return CALIBRATION;
		}
	}
	return 0;                                            // if we aren't within the menu limits, return 0 (no button press / accidental press)
}

void execute_button_command(int which_button){
	switch(which_button){                                                                    // do what the button needs to do
		case 1:
			if(!is_plot_on){                                                                 // if the waveform viewer is not ENABLED but the cursor button was pressed
				draw_cursor_warning();
				}else{
				if(!is_cursor_on){                                                           // otherwise, if the waveform viewer is on and the cursor is off
					draw_cursor();                                                           // turn the cursor on
					is_cursor_on = true;
					}else{                                                                   // but if the cursor was already on when the button was pressed
					erase_cursor();                                                      // erase the cursor (turn it off)
					erase_voltage_zone();                                                // and clear the Voltmeter area (clear "Cursor: xx.xxxV")
					is_cursor_on = false;
				}
			}
			break;
		case 2:
			is_holding = !is_holding;                                                        // if HOLD is off, turn it on and vice-versa
			break;
		case 3:
			if(is_plot_on){                                                                  // if the waveform viewer is on
				is_plot_on = false;                                                          // turn it off
				is_cursor_on = false;                                                        // turn off the cursor
				is_holding = false;                                                          // turn off HOLD
				erase_cursor();                                                              // erase the cursor
				erase_voltage_zone();                                                        // clear the Voltmeter area
				clear_plot();                                                                // clear the waveform viewer area (erase the plot)
			}else{                                                                           // otherwise,
				clear_plot();                                                                // clear the waveform viewer area (erase the text to make space for the viewer)
				is_plot_on = true;                                                           // and turn on the plotting
			}
		break;
		case 4:
		if(!is_plot_on){                                                                 // if the waveform viewer is not ENABLED but the toggle button was pressed
				draw_toggle_warning();
				}else{                                                                           // otherwise, change from the simple waveform viewer type to the one with samples (vertical lines) and vice-versa
				if(!is_digital_line){
					is_digital_line = true;
					toggle_plot();
					plot_points_digital(plot_points, index, max_value, min_value);
					if(is_cursor_on){                                                        // if the cursor was on, erase and redraw it
						erase_cursor();
						draw_cursor();
					}
					}else{
					is_digital_line = false;
					toggle_plot();
					plot_points_line(plot_points, index, max_value, min_value);
					if(is_cursor_on){                                                        // if the cursor was on, erase and redraw it
						erase_cursor();
						draw_cursor();
					}
				}
			}
			break;
		case 5:
			is_calibrated = true;
			ADC_get_max_value();
			display_fill_color(COLOR_BLACK);  
			break;
	}
}

void touchscreen_process_commad(){
	uint16_t x = touch_spi_transfer(GET_X_COMMAND);                                          // get touchscreen x
	uint16_t y = touch_spi_transfer(GET_Y_COMMAND);                                          // get touchscreen y

	uint8_t which_button = check_touch_buttons(x, y);                                        // check if it is a button press or not
	execute_button_command(which_button);                                                    // execute the touch button command
}