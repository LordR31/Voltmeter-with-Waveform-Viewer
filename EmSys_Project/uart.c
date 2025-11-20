#include "uart.h"

extern bool uart_command_pending;
volatile char command_buffer[CMD_BUFFER_SIZE];
volatile unsigned char received_byte;
volatile uint8_t usart_index = 0;

extern bool is_plot_on;
extern float min_value;
extern float max_value;
extern float plot_points[];
extern int index;

bool usart_adc = false;

void USART0_Init(unsigned int ubrr){
	/* Set baud rate */
	UBRR0H = (unsigned char)(ubrr>>8);
	UBRR0L = (unsigned char)ubrr;
	
	UCSR0B = (1 << RXEN0) | (1 << TXEN0);
	//UCSR0B |= (1 << RXCIE0); // int
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void USART_Transmit(unsigned char data){
	while ( !( UCSR0A & (1<<UDRE0)) ); // empty the buffer
	UDR0 = data;
}

void USART_Send_Msg(char *msg){
	for (int i = 0; msg[i] != '\0'; i++)
		USART_Transmit(msg[i]);
} 

unsigned char USART_Read(void) {
	return UDR0;
}

uint8_t USART_Data_Available(void) {
	return (UCSR0A & (1 << RXC0));
}

void USART_Process_Command(void) {
	command_buffer[usart_index] = '\0';  // terminate string

	if (strcmp(command_buffer, "adc_on") == 0) {
			usart_adc = true;
			set_ADC_state(true);
		}else if (strcmp(command_buffer, "adc_off") == 0) {
			usart_adc = false;
			set_ADC_state(false);
		}else if (strcmp(command_buffer, "wave") == 0) {
			display_fill_color(COLOR_BLACK);
			execute_button_command(3);
		}else if (strcmp(command_buffer, "toggle") == 0) {
			execute_button_command(4);
		}else if (strcmp(command_buffer, "hold") == 0){
			execute_button_command(2);
		}else if (strcmp(command_buffer, "cursor") == 0){
			execute_button_command(1);
		}else if (strcmp(command_buffer, "help") == 0) {
			USART_Send_Msg("This is the list of available commands:\r\n\
			adc_on    -> turns on the ADC\r\n\
			adc_off   -> turns off the ADC\r\n\
			wave      -> turns on waveform viewer\r\n\
			toggle    -> toggles waveform viewer mode\r\n\
			hold      -> toggles the hold function\r\n\
			cursor    -> toggles the cursor On/Off\
			");
	}

	usart_index = 0;  // reset buffer for next command
}

void USART_CheckAndHandle(void) {
	while (USART_Data_Available()) {
		unsigned char c = USART_Read();

		if (c == '\r' || c == '\n') {
			if (usart_index > 0) {
				USART_Process_Command();
			}
			} else if (usart_index < CMD_BUFFER_SIZE - 1) {
			command_buffer[usart_index++] = c;
		}
	}
}








/*
void USART_Process_Command(unsigned char c) {
    if (c == '\r' || c == '\n') {
        command_buffer[usart_index] = '\0';
        usart_index = 0;

        if (strcmp(command_buffer, "adc_on") == 0) {
            set_ADC_state(true);
        } else if (strcmp(command_buffer, "adc_off") == 0) {
            set_ADC_state(false);
        } else if (strcmp(command_buffer, "wave") == 0) {
            execute_button_command(2);
        } else if (strcmp(command_buffer, "toggle") == 0) {
            execute_button_command(3);
        } else if (strcmp(command_buffer, "help") == 0) {
            USART_Send_Msg("Commands:\r\nadc_on\r\nadc_off\r\nwave\r\ntoggle\r\n");
        }
    } else if (usart_index < CMD_BUFFER_SIZE - 1) {
        command_buffer[usart_index++] = c;
    }
}*/
/*
ISR(USART0_RX_vect) {
	char c = UDR0; 
	USART_Transmit(c);  // echo back immediately
	if (c == '\r' || c == '\n') {
		command_buffer[usart_index] = '\0';
		usart_index = 0;
		uart_command_pending = true;  
	}else if (usart_index < CMD_BUFFER_SIZE - 1)
		command_buffer[usart_index++] = c;
}*/