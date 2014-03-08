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
#include "gyro.h"

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
	gyroInit ();
	set_sleep_mode (SLEEP_MODE_IDLE);

	printf ("initialization done\n");

	/* global interrupt enable */
	sei ();
	gyroStart ();

	//timerStart ();
	while (1) {
		//sleepwhile (!timerHit ());
		//printf ("running for %u seconds\n", seconds);

		sleepwhile (!gyroRead ());
		sleepwhile (twr.status == TWST_WAIT);
		switch (twr.status) {
			case TWST_OK: {
				volatile const int16_t *val = gyroGet ();
				printf ("%i/%i/%i\n", val[0], val[1], val[2]);
				break;
			}

			case TWST_ERR:
				goto fail;
				break;
		}
	}
	//timerStop ();
fail:
	printf ("fail\n");

	/* global interrupt disable */
	cli ();

	while (1);
}

