#include "uart.h"
#include <avr/io.h>
int uart_putc(unsigned char c) {
	while (!(UCSRA & (1<<UDRE))) {
	}                             
	UDR = c;                      /* sende Zeichen */
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
	UCSRB |= (1<<TXEN) | ( 1 << RXEN );                // UART TX einschalten
	UCSRC |= (1<<URSEL)|(3<<UCSZ0);    // Asynchron 8N1 
	UBRRH = UBRR_VAL >> 8;
	UBRRL = UBRR_VAL & 0xFF;
}

