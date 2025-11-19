#include "buttons.h"
#include <avr/interrupt.h>

#define CMD_BUFFER_SIZE 32
extern bool start_button_pressed;
extern bool voltage_button_pressed;
extern bool right_button_pressed;
extern bool left_button_pressed;
extern bool touch_pending;

ISR(INT4_vect) {
	start_button_pressed = true;
}

ISR(INT3_vect){
	voltage_button_pressed = true;
}

ISR(INT1_vect){
	right_button_pressed = true;
}

ISR(INT0_vect){
	left_button_pressed = true;
}

ISR(INT2_vect) {
	touch_pending = true;
}

void button_init(char port, int pin, int isc0, int isc1, int interrupt){
	switch (port){
	case 'E':
		DDRE &= ~(1 << pin);                    // set pin as input
		PORTE |= (1 << pin);                    // set pull-up resistor
		EICRB &= ~((1 << (2 * interrupt + 1)) | (1 << (2 * interrupt))); // clear both ISC regs
		if (isc0)
			EICRB |= (1 << (2 * interrupt));
		if (isc1)
			EICRB |= (1 << (2 * interrupt + 1));
		break;
	case 'D':
		DDRD &= ~(1 << pin);
		PORTD |= (1 << pin);
		EICRA &= ~((1 << (2 * interrupt + 1)) | (1 << (2 * interrupt))); // clear regs for ISC
		if (isc0)
			EICRA |= (1 << (2 * interrupt));
		if (isc1)
			EICRA |= (1 << (2 * interrupt + 1));
		break;
	}
	EIMSK |= (1 << interrupt);                  // enable interrupt
}