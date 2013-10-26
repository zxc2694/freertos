#ifndef IO_SET_SERIAL_H
#define IO_SET_SERIAL_H

void Init_Serial();
void USART2_IRQHandler();
void send_byte(char ch);
char receive_byte();

#endif
