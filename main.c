/* cpu runs at 1mhz */
#define F_CPU 1000000

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/delay.h>
#include <stdio.h>
#include <stdbool.h>

#include "i2c.h"
#include "uart.h"
#include "timer.h"

static void ledInit () {
	/* set led1,led2 to output */
	DDRB |= (1 << PB6) | (1 << PB7);
	/* set led3,led4,led5,led6 to output */
	DDRD |= (1 << PD2) | (1 << PD3) | (1 << PD4) | (1 << PD5);
}

/* show data with leds */
static void ledShow (const unsigned char val) {
	PORTB = (PORTB & ~((1 << PB6) | (1 << PB7))) | ((val & 0x3) << PB6);
	PORTD = (PORTD & ~((1 << PD2) | (1 << PD3) | (1 << PD4) | (1 << PD5))) | (((val >> 2) & 0xf) << PD2);
}

static void cpuInit () {
	/* enter change prescaler mode */
	CLKPR = CLKPCE << 1;
	/* write new prescaler = 8 (i.e. 1Mhz clock frequency) */
	CLKPR = 0b00000011;
}

#define sleepwhile(cond) \
	sleep_enable (); \
	while (cond) { sleep_cpu (); } \
	sleep_disable ();

int main () {
	cpuInit ();
	ledInit ();
	twInit ();
	uartInit ();
	set_sleep_mode (SLEEP_MODE_IDLE);
	
	printf ("initialization done\n");

	/* global interrupt enable */
	sei ();
	/* disable power-down-mode */
	if (!twWrite (LIS302DL, LIS302DL_CTRLREG1, 0b01000111)) {
		printf ("cannot start write\n");
	}
	sleepwhile (twr.status == TWST_WAIT);
	printf ("final twi status was %i\n", twr.status);

	timerStart ();
	unsigned char seconds = 0;
	while (1) {
		uint8_t val[6];

		sleepwhile (!timerHit ());
		++seconds;
		printf ("running for %u seconds\n", seconds);

		if (!twReadMulti (LIS302DL, 0x28, val, 6)) {
			printf ("cannot start read\n");
		}
		sleepwhile (twr.status == TWST_WAIT);
		printf ("%i/%i/%i\n", (int8_t) val[1], (int8_t) val[3],
				(int8_t) val[5]);
	}
	timerStop ();

	/* global interrupt disable */
	cli ();

	while (1);
}

