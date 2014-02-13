#include <stdio.h>
#include <util/twi.h>
#include <avr/interrupt.h>

#include "i2c.h"

volatile twReq twr;

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

static void twWaitRaw () {
	while (!(TWCR & (1 << TWINT)));
}

static bool twWriteRaw (const uint8_t data) {
	TWDR = data;
	if (TWCR & (1 << TWWC)) {
		printf("write collision\n");
		return false;
	} else {
		return true;
	}
}

void twInit () {
	/* set scl to 3.6 kHz (at 1Mhz CPU speed)*/
	TWBR = 2;
	TWSR |= 0x3; /* set prescaler to 64 */

	twr.mode = TWM_INVALID;
	twr.status = TWST_ERR;
}

/*	high-level write
 */
bool twWrite (const uint8_t address, const uint8_t subaddress,
		const uint8_t data) {
	/* do not start if request is pending */
	if (twr.status == TWST_WAIT) {
		return false;
	}

	twr.mode = TWM_WRITE;
	twr.address = address;
	twr.subaddress = subaddress;
	twr.data = data;
	twr.step = 0;
	twr.status = TWST_WAIT;
	/* wait for stop finish */
	while (TW_STATUS != 0xf8);
	twStartRaw ();

	return true;
}

/*	high-level read for multiple bytes/addresses
 */
bool twReadMulti (const uint8_t address, const uint8_t subaddress,
		uint8_t * const retData, const uint8_t count) {
	/* do not start if request is pending */
	if (twr.status == TWST_WAIT) {
		return false;
	}

	twr.mode = TWM_READ_MULTI;
	twr.address = address;
	twr.subaddress = subaddress;
	twr.retData = retData;
	twr.count = count;
	twr.i = 0;
	twr.step = 0;
	twr.status = TWST_WAIT;
	/* wait for stop finish */
	while (TW_STATUS != 0xf8);
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
			} else {
				twr.status = TWST_ERR;
			}
			break;

		case 1:
			if (TW_STATUS == TW_MT_SLA_ACK) {
				twWriteRaw (twr.subaddress);
				twFlushRaw ();
			} else {
				twr.status = TWST_ERR;
			}
			break;

		case 2:
			if (TW_STATUS == TW_MT_DATA_ACK) {
				twWriteRaw (twr.data);
				twFlushRaw ();
			} else {
				twr.status = TWST_ERR;
			}
			break;

		case 3:
			if (TW_STATUS == TW_MT_DATA_ACK) {
				twStopRaw ();
				twr.status = TWST_OK;
			} else {
				twr.status = TWST_ERR;
			}
			break;

		default:
			printf ("nope\n");
			break;
	}
	++twr.step;
}

/*	handle interrupt, read request
 */
static void twIntReadMulti () {
	switch (twr.step) {
		case 0:
			if (TW_STATUS == TW_START) {
				/* write device address */
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
				/* send repeated start */
				twStartRaw ();
				++twr.step;
			} else {
				twr.status = TWST_ERR;
			}
			break;

		case 3:
			if (TW_STATUS == TW_REP_START) {
				/* now start the actual read request */
				twWriteRaw (twr.address | TW_READ);
				twFlushRaw ();
				++twr.step;
			} else {
				twr.status = TWST_ERR;
			}
			break;

		case 4:
			if (TW_STATUS == TW_MR_SLA_ACK) {
				/* send master ack if next data block is received */
				twFlushContRaw ();
				++twr.step;
			} else {
				twr.status = TWST_ERR;
			}

		case 5:
			if (TW_STATUS == TW_MR_DATA_ACK) {
				twr.retData[twr.i] = TWDR;
				++twr.i;
				if (twr.i < twr.count-1) {
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
			}
			break;

		case 6:
			if (TW_STATUS == TW_MR_DATA_NACK) {
				/* receive final byte, send stop */
				twr.retData[twr.i] = TWDR;
				twStopRaw ();
				twr.status = TWST_OK;
			} else {
				twr.status = TWST_ERR;
			}
			break;

		default:
			printf ("twIntReadMulti: nope\n");
			break;
	}
}

ISR(TWI_vect) {
	switch (twr.mode) {
		case TWM_WRITE:
			twIntWrite ();
			break;

		case TWM_READ_MULTI:
			twIntReadMulti ();
			break;

		default:
			printf ("nope\n");
			break;
	}
}
