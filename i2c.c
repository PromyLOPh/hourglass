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

#include <stdio.h>
#include <util/twi.h>
#include <avr/interrupt.h>
#include <stdlib.h>

#include "i2c.h"
#include "common.h"

twReq twr;

static void twStartRaw () {
	/* disable stop, enable interrupt, reset twint, enable start, enable i2c */
	TWCR = (TWCR & ~(1 << TWSTO)) | (1 << TWIE) | (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
}

static void twStopRaw () {
	/* disable start, enable interrupt, reset twint, enable stop, enable i2c */
	TWCR = (TWCR & ~(1 << TWSTA)) | (1 << TWIE) | (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
}

static void twFlushRaw () {
	/* disable start/stop, enable interrupt, reset twint, enable i2c */
	TWCR = (TWCR & ~((1 << TWSTA) | (1 << TWSTO) | (1 << TWEA))) | (1 << TWIE) | (1 << TWINT) | (1 << TWEN);
}

/* flush and send master ack */
static void twFlushContRaw () {
	/* disable start/stop, enable interrupt, reset twint, enable i2c, send master ack */
	TWCR = (TWCR & ~((1 << TWSTA) | (1 << TWSTO))) | (1 << TWIE) | (1 << TWINT) | (1 << TWEN) | (1 << TWEA);
}

#if 0
/* unused */
static void twWaitRaw () {
	while (!(TWCR & (1 << TWINT)));
}
#endif

static bool twWriteRaw (const uint8_t data) {
	TWDR = data;
	if (TWCR & (1 << TWWC)) {
		return false;
	} else {
		return true;
	}
}

void twInit () {
#if F_CPU == 1000000
	/* set scl to 50 kHz */
	TWBR = 2;
	/* prescaler 1 */
	TWSR &= ~((1 << TWPS1) | (1 << TWPS0));
#elif F_CPU == 4000000
	/* set scl to 50 kHz ? */
	TWBR = 32;
	/* prescaler 1 */
	TWSR &= ~((1 << TWPS1) | (1 << TWPS0));
#elif F_CPU == 8000000
	/* set scl to 100 kHz */
	TWBR = 32;
	/* prescaler 1 */
	TWSR &= ~((1 << TWPS1) | (1 << TWPS0));
#else
#error "cpu speed not supported"
#endif

	twr.mode = TWM_INVALID;
	twr.status = TWST_OK;
}

/*	high-level write, returns false if bus is busy
 */
bool twRequest (const twMode mode, const uint8_t address,
		const uint8_t subaddress, uint8_t * const data, const uint8_t count) {
	/* busy */
	if (twr.status != TWST_OK) {
		return false;
	}

	assert (count > 0);
	assert (data != NULL);

	twr.mode = mode;
	twr.address = address;
	twr.subaddress = subaddress;
	twr.data = data;
	twr.count = count;
	twr.i = 0;
	twr.step = 0;
	twr.status = TWST_WAIT;
	/* wait for stop finish; there is no interrupt generated for this */
	while (TW_STATUS != 0xf8 || TWCR & (1 << TWSTO));
	twStartRaw ();

	return true;
}

/*	handle interrupt, write request
 */
static void twIntWrite () {
	switch (twr.step) {
		case 0:
			if (TW_STATUS == TW_START) {
				twWriteRaw (twr.address | TW_WRITE);
				twFlushRaw ();
				++twr.step;
			} else {
				twr.status = TWST_ERR;
			}
			break;

		case 1:
			if (TW_STATUS == TW_MT_SLA_ACK) {
				/* write subaddress, enable auto-increment */
				twWriteRaw ((1 << 7) | twr.subaddress);
				twFlushRaw ();
				++twr.step;
			} else {
				twr.status = TWST_ERR;
			}
			break;

		case 2:
			if (TW_STATUS == TW_MT_DATA_ACK) {
				twWriteRaw (twr.data[twr.i]);
				++twr.i;
				twFlushRaw ();
				if (twr.i >= twr.count) {
					++twr.step;
				}
			} else {
				twr.status = TWST_ERR;
			}
			break;

		case 3:
			if (TW_STATUS == TW_MT_DATA_ACK) {
				twStopRaw ();
				twr.status = TWST_OK;
				enableWakeup (WAKE_I2C);
				++twr.step;
			} else {
				twr.status = TWST_ERR;
			}
			break;

		default:
			assert (0 && "nope");
			break;
	}
}

/*	handle interrupt, read request
 */
static void twIntRead () {
	uint8_t status = TW_STATUS;
	switch (twr.step) {
		case 0:
			if (status == TW_START) {
				/* write device address */
				twWriteRaw (twr.address | TW_WRITE);
				twFlushRaw ();
				++twr.step;
			} else {
				twr.status = TWST_ERR;
				twr.error = status;
			}
			break;

		case 1:
			if (status == TW_MT_SLA_ACK) {
				/* write subaddress, enable auto-increment */
				twWriteRaw ((1 << 7) | twr.subaddress);
				twFlushRaw ();
				++twr.step;
			} else {
				twr.status = TWST_ERR;
				twr.error = status;
			}
			break;

		case 2:
			if (status == TW_MT_DATA_ACK) {
				/* send repeated start */
				twStartRaw ();
				++twr.step;
			} else {
				twr.status = TWST_ERR;
				twr.error = status;
			}
			break;

		case 3:
			if (status == TW_REP_START) {
				/* now start the actual read request */
				twWriteRaw (twr.address | TW_READ);
				twFlushRaw ();
				++twr.step;
			} else {
				twr.status = TWST_ERR;
				twr.error = status;
			}
			break;

		case 4:
			if (status == TW_MR_SLA_ACK) {
				/* send master ack if next data block is received */
				twFlushContRaw ();
				++twr.step;
			} else {
				twr.status = TWST_ERR;
				twr.error = status;
			}
			break;

		case 5:
			if (status == TW_MR_DATA_ACK) {
				twr.data[twr.i] = TWDR;
				++twr.i;
				if (twr.i < twr.count) {
					/* read another byte, not the last one */
					twFlushContRaw ();
					/* step stays the same */
				} else {
					/* read last byte, send master nack */
					twFlushRaw ();
					++twr.step;
				}
			} else {
				twr.status = TWST_ERR;
				twr.error = status;
			}
			break;

		case 6:
			if (status == TW_MR_DATA_NACK) {
				twStopRaw ();
				twr.status = TWST_OK;
				enableWakeup (WAKE_I2C);
				++twr.step;
			} else {
				twr.status = TWST_ERR;
				twr.error = status;
			}
			break;

		default:
			assert (0 && "twIntRead: nope\n");
			break;
	}
}

ISR(TWI_vect) {
	switch (twr.mode) {
		case TWM_WRITE:
			twIntWrite ();
			break;

		case TWM_READ:
			twIntRead ();
			break;

		default:
			assert (0 && "nope\n");
			break;
	}
	assert (twr.status != TWST_ERR);
}

