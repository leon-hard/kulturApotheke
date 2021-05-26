#include "uart.h"
#include <avr/io.h>

int uart_putc(unsigned char c) {
	while (!(UCSR0A & (1<<UDRE0))) {
	}                             
	UDR0 = c;                      /* sende Zeichen */
	return 0;
}

void uart_puts (char *s) {
	while (*s) {   
		uart_putc(*s);
		s++;
	}
}
 
void uart_init(void)
{
	UCSR0B |= (1<<TXEN0) | ( 1 << RXEN0 );                // UART TX einschalten
	//UCSR0C |= (1<<URSEL)|(3<<UCSZ0);    // Asynchron 8N1
	UCSR0C = (1<<UCSZ01)|(1<<UCSZ00);
	UBRR0 = UBRR_VAL;
	//UBRRH = UBRR_VAL >> 8;
	//UBRRL = UBRR_VAL & 0xFF;
}

