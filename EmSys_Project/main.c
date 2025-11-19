#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdbool.h>

#include "adc.h"
#include "buttons.h"
#include "timers.h"
#include "spi.h"
#include "display.h"
#include "touch.h"
#include "globals.h"
#include "uart.h"

#define DEBUG_MODE

int main(void){
	Timer1_Init(); 					                                                                     // init timer 1 for delay_ms and delay_us
	button_init('E', START_BUTTON_PIN, ISC0, ISC1, EXT_INT4);                                            // init start button
	button_init('D', CURSOR_LEFT_PIN, ISC0, ISC1, EXT_INT0);                                             // init cursor left button
	button_init('D', CURSOR_RIGHT_PIN, ISC0, ISC1, EXT_INT1);                                            // init cursor right button
	button_init('D', VOLTAGE_TYPE_TOGGLE_PIN, ISC0, ISC1, EXT_INT3);                                     // init toggle voltage type button
	ADC_Init(); 					                                                                     // init the ADC
	SPI_init(); 					                                                                     // init SPI 
	Touch_SPI_Init(); 				                                                                     // init the pins for the touchscreen side (should merge SPI and Touch_SPI)
	Touch_IRQ_Init(); 				                                                                     // init Touch Interrupt pin
	Display_Init(); 				                                                                     // init the display
	sei();                                                                                               // enable interrupts                                                               
	
	display_set_rotation(LANDSCAPE);                                                                     // set display rotaion
	display_fill_color(COLOR_BLACK);                                                                     // clear screen
	
#ifdef DEBUG_MODE
	USART0_Init(MYUBRR);
#endif 

	while (1){
#ifdef DEBUG_MODE
	USART_CheckAndHandle();
#endif
		
		if (start_button_pressed) {                                                                      // if the start button is pressed	
			is_system_on = !is_system_on;                                                                // toggle system state
			display_fill_color(COLOR_BLACK);                                                             // clear the screen
#ifdef DEBUG_MODE
			set_ADC_state(false); 
#else
			set_ADC_state(is_system_on);                                                                 // set ADC state based on system state
#endif
			start_button_pressed = false;                                                                // and clear the flag
		}
		
		if (voltage_button_pressed) {                                                                    // if the voltage mode button is pressed

			is_high_voltage = !is_high_voltage;                                                          // toggle voltage type
#ifdef DEBUG_MODE
			char message[32];
			strcpy(message, "Switched to ");
			if(is_high_voltage)
				strcat(message, "High Voltage Mode!\r\n");
			else
				strcat(message, "Low Voltage Mode!\r\n");
			USART_Send_Msg(message);
#else
#endif			
			voltage_button_pressed = false;                                                              // and clear the flag
		}
		if(is_system_on){		                                                                         // if the system is on	
			if(!is_calibrated){
				draw_calibration_ui();
				
				if (touch_pending) {                                                                         // if a touch is sensed
					touch_pending = false;                                                                   // clear the flag
					touchscreen_process_commad();                                                            // and process the command
				}
			}else{
			draw_ui();                                                                                   // draw the UI
			draw_voltmeter(is_cursor_on);                                                                // display the voltmeter text
			draw_voltage_type(is_high_voltage);                                                          // display the voltage type
			
			if (touch_pending) {                                                                         // if a touch is sensed
				touch_pending = false;                                                                   // clear the flag
				touchscreen_process_commad();                                                            // and process the command
			}
			
			if(is_adc_on){
				voltage_value = ADC_measure(is_high_voltage);                                                // get voltage value from adc, based on voltage type (high or low)
				if(!is_holding) {                                                                            // if HOLD is not on, record the values (up to the 25th value)
					if(index < 25){
						plot_points[index] = voltage_value;                                                  // add the voltage values to the end of the array until array is full
						index++;
					}else
						insert_plot_points(plot_points, voltage_value);                                      // insert the points into the plot_points array by disarding the first point (oldest point)
				}
				min_value = plot_points[0];
				max_value = plot_points[0];

				for(int i = 0; i < index; i++){                                                              // get min and max from the recorded points for plot scaling
					if(plot_points[i] < min_value)         
						min_value = plot_points[i];
					if(plot_points[i] > max_value)    
						max_value = plot_points[i];
				}
			
				if(!is_holding & is_plot_on){                                                                // if HOLD is off and the waveform viewer is enabled, keep updating it when new points are recorded
					if(is_digital_line)                                                                      // if it's set to digital
						plot_points_digital(plot_points, index, max_value, min_value);                       // display the digital mode 
					else                                                                                     // otherwise
						plot_points_line(plot_points, index, max_value, min_value);                          // keep it analogue
				}
			
				print_voltage(is_cursor_on, voltage_value);                                                   // display the voltage value
				print_min_max_voltage(is_plot_on, min_value, max_value);                                      // display min max

				if (is_cursor_on) {                                                          
					if (right_button_pressed) {                                                              // move cursor right button
						move_cursor(1);                                                                      // move the cursor to the right one point
						right_button_pressed = false;
					}

					if (left_button_pressed) {                                                               // move cursor left button
						move_cursor(-1);                                                                     // move the cursor to the left one point
						left_button_pressed = false;
					}
					cursor_voltage = get_cursor_voltage();                                                   // get the voltage of the point at the cursor position
					print_cursor_voltage(cursor_voltage);
				}
				draw_indicator_leds(voltage_value, is_high_voltage);                                         // draw the voltage level indicator "LEDs"
			}
		}}else{                                                                                           // IF the system is off
			draw_power_on_screen();                                                                      // draw the power on screen
		}
	}
}

