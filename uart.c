#include <stdio.h>
#include <avr/io.h>

#include "uart.h"

/* blocking uart send
 */
static void uartSend (unsigned char data) {
	/* Wait for empty transmit buffer */
	while (!( UCSR0A & (1<<UDRE0)));
	/* Put data into buffer, sends the data */
	UDR0 = data;
}

static int uartPutc (char c, FILE *stream) {
	if (c == '\n') {
		uartSend ('\r');	
	}
	uartSend (c);
	return 0;
}

static FILE mystdout = FDEV_SETUP_STREAM (uartPutc, NULL, _FDEV_SETUP_WRITE);

void uartInit () {
	/* Set baud rate (9600, double speed, at 1mhz) */
	UBRR0H = 0;
	UBRR0L = 12;
	/* enable double speed mode */
	UCSR0A = (1 << U2X0);
	/* Enable receiver and transmitter */
	UCSR0B = (1<<RXEN0)|(1<<TXEN0);
	/* Set frame format: 8 data, 1 stop bit, even parity */
	UCSR0C = (1<<UPM01) | (0 << UPM00) | (0<<USBS0)|(3<<UCSZ00);

	/* redirect stdout */
	stdout = &mystdout;
}

static unsigned char uartReceive () {
	/* Wait for data to be received */
	while ( !(UCSR0A & (1<<RXC0)) );
	/* Get and return received data from buffer */
	return UDR0;
}

