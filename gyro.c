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
#define L3GD20_OUTZ 0x2c

/* raw z value */
static volatile int16_t zval = 0;
/* accumulated z value */
static int32_t zaccum = 0;
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
	 * disable power-down-mode, enable z
	 * defaults
	 * low-active (does not work?), push-pull, drdy on int2
	 * select 500dps
	 */
	uint8_t data[] = {0b00001100, 0b0, 0b00101000, 0b00010000};

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

	if (zaccum > (1 << shift)) {
		const uint32_t a = abs (zaccum);
		zticks += a >> shift;
		/* mask shift bits */
		zaccum -= a & (~0x3ff);
	} else if (zaccum < -(1 << shift)) {
		const uint32_t a = abs (zaccum);
		zticks -= a >> shift;
		zaccum += a & (~0x3ff);
	}
}

/*	process gyro sensor data, returns true if new data is available
 */
bool gyroProcess () {
	if (reading) {
		if (twr.status == TWST_OK) {
			/* new data transfered, process it */
			/* poor man's noise filter */
			if (abs (zval) > 64) {
				zaccum += zval;
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
			if (!twRequest (TWM_READ, L3GD20, L3GD20_OUTZ,
					(uint8_t *) &zval, sizeof (zval))) {
				printf ("cannot start read\n");
			} else {
				reading = true;
			}
		}
	}

	return false;
}

const int32_t gyroGetZAccum () {
	return zaccum;
}

void gyroResetZAccum () {
	zaccum = 0;
}

volatile const int16_t gyroGetZRaw () {
	return zval;
}

const int8_t gyroGetZTicks () {
	return zticks;
}

