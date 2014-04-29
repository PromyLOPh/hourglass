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

/* raw values */
static volatile int16_t val[3] = {0, 0, 0};
/* accumulated values */
static int32_t accum[3] = {0, 0, 0};
/* calculated zticks */
static int8_t zticks = 0;
/* currently reading from i2c */
static bool reading = false;

/* data ready interrupt
 */
ISR(PCINT0_vect) {
	/* empty */
}

void gyroInit () {
	/* set PB1 to input, with pull-up */
	DDRB = DDRB & ~((1 << DDB1));
	PORTB = PORTB | (1 << PORTB1);
	/* enable interrupt PCI0 */
	PCICR = PCICR | (1 << PCIE0);
	/* enable interrupts on PB1/PCINT1 */
	PCMSK0 = (1 << PCINT1);
}

/* XXX: make nonblocking */
void gyroStart () {
	/* configuration:
	 * disable power-down-mode
	 * defaults
	 * low-active (does not work?), push-pull, drdy on int2
	 * select 500dps
	 */
	uint8_t data[] = {0b00001111, 0b0, 0b00101000, 0b00010000};

	if (!twRequest (TWM_WRITE, L3GD20, L3GD20_CTRLREG1, data,
			sizeof (data)/sizeof (*data))) {
		printf ("cannot start write\n");
	}
	sleepwhile (twr.status == TWST_WAIT);
	printf ("final twi status was %i\n", twr.status);
}

/*	calculate ticks for z rotation
 */
static void gyroProcessTicks () {
	const uint8_t shift = 14;
	const int32_t zval = accum[2];

	if (zval > (1 << shift)) {
		const uint32_t a = abs (zval);
		zticks += a >> shift;
		/* mask shift bits */
		accum[2] -= a & (~0x3ff);
	} else if (zval < -(1 << shift)) {
		const uint32_t a = abs (zval);
		zticks -= a >> shift;
		accum[2] += a & (~0x3ff);
	}
}

/*	process gyro sensor data, returns true if new data is available
 */
bool gyroProcess () {
	if (reading) {
		if (twr.status == TWST_OK) {
			/* new data transfered, process it */
			for (uint8_t i = 0; i < sizeof (accum)/sizeof (*accum); i++) {
				/* poor man's noise filter */
				if (abs (val[i]) > 64) {
					accum[i] += val[i];
				}
			}
			gyroProcessTicks ();
			reading = false;
			return true;
		} else if (twr.status == TWST_ERR) {
			printf ("gyro i2c error\n");
			reading = false;
		}
	} else {
		if (((PINB >> PINB1) & 0x1) && twr.status != TWST_WAIT) {
			/* new data available in device buffer and bus is free */
			if (!twRequest (TWM_READ, L3GD20, 0x28, (uint8_t *) val, 6)) {
				printf ("cannot start read\n");
			} else {
				reading = true;
			}
		}
	}

	return false;
}

const int32_t *gyroGetAccum () {
	return accum;
}

void gyroResetAccum () {
	memset (accum, 0, sizeof (accum));
}

volatile const int16_t *gyroGetRaw () {
	return val;
}

const int8_t gyroGetZTicks () {
	return zticks;
}

