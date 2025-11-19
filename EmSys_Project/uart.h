#ifndef UART_H_
#define UART_H_

#include <avr/io.h>
#include <string.h>
#include <stdbool.h>

#include "timers.h"
#include "touch.h"

#define F_CPU 16000000UL
#define BAUD 9600
#define MYUBRR ((F_CPU / (16UL * BAUD)) - 1)

#define CMD_BUFFER_SIZE 32

void USART0_Init(unsigned int ubrr);
void USART_Transmit(unsigned char data);
void USART_Send_Msg(char *message);
unsigned char USART_Read(void);
uint8_t USART_Data_Available(void);
void USART_Process_Command();
void USART_CheckAndHandle(void) ;
#endif /* UART_H_ */