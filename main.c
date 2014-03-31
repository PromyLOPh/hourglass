#include "common.h"

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
#include "accel.h"

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

int main () {
	cpuInit ();
	ledInit ();
	twInit ();
	uartInit ();
	gyroInit ();
	accelInit ();
	set_sleep_mode (SLEEP_MODE_IDLE);

	printf ("initialization done\n");

	/* global interrupt enable */
	sei ();
	gyroStart ();
	accelStart ();

	timerStart ();
	bool checkGyro = false;
	while (1) {
		while (!timerHit ()) {
			/* round-robin to prevent starvation */
			if (checkGyro) {
				gyroProcess ();
				accelProcess();
			} else {
				accelProcess();
				gyroProcess ();
			}
			checkGyro = !checkGyro;
			sleep_enable ();
			sleep_cpu ();
			sleep_disable ();
		}
		volatile const int16_t *gyroval = gyroGetAngle ();
		volatile const int8_t *accelval = accelGet ();
		printf ("%i/%i/%i - %i/%i/%i\n", gyroval[0], gyroval[1], gyroval[2],
				accelval[1], accelval[3], accelval[5]);
		gyroResetAngle ();
	}
	timerStop ();

	/* global interrupt disable */
	cli ();

	while (1);
}

