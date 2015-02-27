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

/*	We use two freefall wakup thresholds:
 *	1) Shake detection with >= 127 (~2g), actual value is ignored
 *	2) Horizon detection with <= 60 (~1g) for 100ms, if the interrupt is
 *	   asserted the actual value has to be read to determine the horizon
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/atomic.h>

#include "i2c.h"
#include "accel.h"

/* device address */
#define LIS302DL 0b00111000
/* registers */
#define LIS302DL_WHOAMI 0xf
#define LIS302DL_CTRLREG1 0x20
#define LIS302DL_CTRLREG2 0x21
#define LIS302DL_CTRLREG3 0x22
#define LIS302DL_UNUSED1 0x28
#define LIS302DL_OUTZ 0x2D
#define LIS302DL_FFWUCFG1 0x30
#define LIS302DL_FFWUTHS1 0x32
#define LIS302DL_FFWUCFG2 0x34
#define LIS302DL_FFWUTHS2 0x36

static int8_t zval;
static uint8_t shakeCount = 0;

/* horizon position */
/* current */
static horizon horizonSign = HORIZON_NONE;
static bool horizonChanged = false;

/* driver state */
#define STOPPED 0
#define START_REQUEST 1
#define STARTING_A 2
#define STARTING_B 3
#define STARTING_C 4
#define STARTING_D 5
#define STARTING_E 6
#define STARTING_F 7
#define STOP_REQUEST 8
#define STOPPING 9
#define READING 10
#define IDLE 11
static uint8_t state = STOPPED;

/* data ready interrupt
 */
ISR(PCINT1_vect) {
	const uint8_t pin = PINC;
	/* low-active */
	const bool int1 = !((pin >> PINC0) & 0x1);
	const bool int2 = !((pin >> PINC1) & 0x1);
	if (int1) {
		enableWakeup (WAKE_ACCEL_HORIZON);
	}
	if (int2) {
		enableWakeup (WAKE_ACCEL_SHAKE);
	}
}

void accelInit () {
	/* set interrupt lines to input */
	DDRC &= ~((1 << DDC0) | (1 << DDC1));
	/* enable interrupt PCI1 for PCINT8/9 */
	PCICR = PCICR | (1 << PCIE1);
	/* enable interrupts from port PC0/PC1 aka PCINT8/PCINT9 */
	PCMSK1 = (1 << PCINT9) | (1 << PCINT8);
}

void accelStart () {
	assert (state == STOPPED);
	state = START_REQUEST;
}

void accelProcess () {
	switch (state) {
		case START_REQUEST: {
			/* configuration:
			 * disable power-down-mode, enable z-axis
			 */
			static uint8_t data[] = {0b01000100};

			if (twRequest (TWM_WRITE, LIS302DL, LIS302DL_CTRLREG1, data,
					sizeof (data)/sizeof (*data))) {
				state = STARTING_A;
			}
			break;
		}

		/* set up ff_wu_1 (horizon detection) */
		case STARTING_A: {
			puts ("setting thresh wu1");
			/* threshold 60 (~1g), duration 100ms (10 steps at 10ms) */
			static uint8_t data[] = {0b00111100, 0b1010};
			if (twRequest (TWM_WRITE, LIS302DL, LIS302DL_FFWUTHS1, data,
					sizeof (data)/sizeof (*data))) {
				state = STARTING_B;
			}
			break;
		}

		case STARTING_B:
			if (shouldWakeup (WAKE_I2C)) {
				disableWakeup (WAKE_I2C);
				puts ("setting events wu1");
				/* or events, enable interrupt on z low event */
				static uint8_t data[] = {0b00010000};
				if (twRequest (TWM_WRITE, LIS302DL, LIS302DL_FFWUCFG1, data,
						sizeof (data)/sizeof (*data))) {
					state = STARTING_C;
				}
			}
			break;

		/* set up ff_wu_2 (shake detection) */
		case STARTING_C: {
			puts ("setting thresh wu2");
			/* threshold 127 (~2g) (duration, next register, not modified) */
			static uint8_t data[] = {0b01111111};
			if (twRequest (TWM_WRITE, LIS302DL, LIS302DL_FFWUTHS2, data,
					sizeof (data)/sizeof (*data))) {
				state = STARTING_D;
			}
			break;
		}

		case STARTING_D:
			if (shouldWakeup (WAKE_I2C)) {
				disableWakeup (WAKE_I2C);
				puts ("setting events wu2");
				/* or events, enable interrupt on z high event (+/-) */
				static uint8_t data[] = {0b00100000};
				if (twRequest (TWM_WRITE, LIS302DL, LIS302DL_FFWUCFG2, data,
						sizeof (data)/sizeof (*data))) {
					state = STARTING_E;
				}
			}
			break;

		case STARTING_E:
			if (shouldWakeup (WAKE_I2C)) {
				disableWakeup (WAKE_I2C);
				puts ("setting int");
				/* push-pull, low-active, FF_WU_1 on int1, FF_WU_2 on int2 */
				static uint8_t data[] = {0b10010001};
				if (twRequest (TWM_WRITE, LIS302DL, LIS302DL_CTRLREG3, data,
						sizeof (data)/sizeof (*data))) {
					state = STARTING_F;
				}
			}
			break;

		case STARTING_F:
			if (shouldWakeup (WAKE_I2C)) {
				disableWakeup (WAKE_I2C);
				puts ("ffwu setup done");
				state = IDLE;
			}
			break;

		case IDLE:
			/* new data available in device buffer and bus is free */
			if (shouldWakeup (WAKE_ACCEL_HORIZON) && twRequest (TWM_READ, LIS302DL,
						LIS302DL_OUTZ, (uint8_t *) &zval, sizeof (zval))) {
				disableWakeup (WAKE_ACCEL_HORIZON);
				state = READING;
			}
			if (shouldWakeup (WAKE_ACCEL_SHAKE)) {
				disableWakeup (WAKE_ACCEL_SHAKE);
				++shakeCount;
			}
			break;

		case READING:
			if (shouldWakeup (WAKE_I2C)) {
				disableWakeup (WAKE_I2C);
				/* the bus might be in use again already */
				//assert (twr.status == TWST_OK);

				if (zval >= 0) {
					if (horizonSign == HORIZON_NEG) {
						horizonChanged = true;
					}
					horizonSign = HORIZON_POS;
				} else {
					if (horizonSign == HORIZON_POS) {
						horizonChanged = true;
					}
					horizonSign = HORIZON_NEG;
				}

				state = IDLE;
			}
			break;

		default:
			assert (0);
			break;
	}
}

horizon accelGetHorizon (bool * const changed) {
	*changed = horizonChanged;
	horizonChanged = false;
	return horizonSign;
}

uint8_t accelGetShakeCount () {
	return shakeCount;
}

void accelResetShakeCount () {
	shakeCount = 0;
}

