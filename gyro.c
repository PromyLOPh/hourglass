/*
Copyright (c) 2014-2015
	Lars-Dominik Braun <lars@6xq.net>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

#include "i2c.h"
#include "gyro.h"

/* device address */
#define L3GD20 0b11010100
/* registers */
#define L3GD20_WHOAMI 0xf
#define L3GD20_CTRLREG1 0x20
#define L3GD20_CTRLREG3 0x22
#define L3GD20_CTRLREG4 0x23
#define L3GD20_CTRLREG5 0x24
#define L3GD20_OUTZ 0x2c

/* raw z value */
static volatile int16_t zval = 0;
/* accumulated z value */
static int32_t zaccum = 0;
/* calculated zticks */
static int16_t zticks = 0;

#define STOPPED 0
#define START_REQUEST 1
#define STARTING 2
#define STOPPING 4
#define READING 5
#define IDLE 6
static uint8_t state = STOPPED;
static bool shouldStop = false;

/* data ready interrupt
 */
ISR(PCINT0_vect) {
	const bool interrupt = (PINB >> PINB1) & 0x1;
	/* high-active */
	if (interrupt) {
		enableWakeup (WAKE_GYRO);
	} else {
		disableWakeup (WAKE_GYRO);
	}
}

void gyroInit () {
	/* set PB1 to input */
	DDRB = DDRB & ~((1 << DDB1));
	/* enable interrupt PCI0 */
	PCICR = PCICR | (1 << PCIE0);
	/* enable interrupts on PB1/PCINT1 */
	PCMSK0 = (1 << PCINT1);
}

void gyroStart () {
	assert (state == STOPPED);
	state = START_REQUEST;
	shouldStop = false;
}

void gyroStop () {
	shouldStop = true;
}

/*	calculate ticks for z rotation
 */
static void gyroProcessTicks () {
	const uint8_t shift = 13;
	const uint32_t max = (1 << shift);
	const uint32_t mask = ~(max-1);

	if (zaccum > (int32_t) max) {
		const uint32_t a = abs (zaccum);
		zticks += a >> shift;
		/* mask shift bits */
		zaccum -= a & mask;
	} else if (zaccum < -((int32_t) max)) {
		const uint32_t a = abs (zaccum);
		zticks -= a >> shift;
		zaccum += a & mask;
	}
}

/*	process gyro sensor data, returns true if new data is available
 */
bool gyroProcess () {
	switch (state) {
		case START_REQUEST: {
			/* configuration:
			 * disable power-down-mode, enable z
			 * defaults
			 * high-active, push-pull, drdy on int2
			 * select 2000dps
			 */
			static uint8_t data[] = {0b00001100, 0b0, 0b00001000, 0b00110000};
			if (twRequest (TWM_WRITE, L3GD20, L3GD20_CTRLREG1, data,
					length (data))) {
				state = STARTING;
			}
			break;
		}

		case STARTING:
			if (shouldWakeup (WAKE_I2C)) {
				disableWakeup (WAKE_I2C);
				state = IDLE;
			}
			break;

		case STOPPING:
			if (shouldWakeup (WAKE_I2C)) {
				disableWakeup (WAKE_I2C);
				state = STOPPED;
			}
			break;

		case READING:
			if (shouldWakeup (WAKE_I2C)) {
				disableWakeup (WAKE_I2C);
				state = IDLE;
				/* the bus might be in use again already */
				//assert (twr.status == TWST_OK);
				/* new data transfered, process it */
				/* poor man's noise filter */
				if (abs (zval) > 64) {
					zaccum += zval;
				}
				gyroProcessTicks ();
				return true;
			}
			break;

		case IDLE:
			if (shouldStop) {
				/* enable power-down mode */
				static uint8_t data[] = {0b00000000};

				if (twRequest (TWM_WRITE, L3GD20, L3GD20_CTRLREG1, data,
						length (data))) {
					state = STOPPING;
				}
			} else if (shouldWakeup (WAKE_GYRO) && twRequest (TWM_READ, L3GD20,
						L3GD20_OUTZ, (uint8_t *) &zval, sizeof (zval))) {
				/* new data available in device buffer and bus is free */
				/* wakeup source is disabled by isr to prevent race condition */
				state = READING;
			}

			break;

		default:
			/* ignore */
			break;
	}

	return false;
}

int32_t gyroGetZAccum () {
	return zaccum;
}

void gyroResetZAccum () {
	zaccum = 0;
}

int16_t gyroGetZRaw () {
	return zval;
}

int16_t gyroGetZTicks () {
	return zticks;
}

void gyroResetZTicks () {
	zticks = 0;
}

