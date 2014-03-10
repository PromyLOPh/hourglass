#include "common.h"

#include <stdio.h>
#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

#include "i2c.h"
#include "gyro.h"

/* max degree +/- per second */
#define GYRO_DPS 500
/* output data rate, hertz */
#define GYRO_FREQ 95
/* millidegree per second and digit, (2*GYRO_DPS+1)/(2^16) ~= 15.274, manual
 * says 17.5, so choose something in between */
#define GYRO_MDPS_PER_DIGIT 16
/* milliangle (in degree) per 32 units, 32 is a relatively small number (not
 * much information lost) and the angle is very to a full integer number,
 * GYRO_MDPS_PER_DIGIT/GYRO_FREQ*32 */
#define GYRO_MANGLE_PER_32 5

/* device address */
#define L3GD20 0b11010100
/* registers */
#define L3GD20_WHOAMI 0xf
#define L3GD20_CTRLREG1 0x20
#define L3GD20_CTRLREG3 0x22
#define L3GD20_CTRLREG4 0x23

/* the first interrupt is lost */
static volatile bool drdy = true;
static volatile int16_t val[3] = {0, 0, 0};
/* current (relative) angle, in millidegree */
static int16_t angle[3] = {0, 0, 0};
/* currently reading from i2c */
static bool reading = false;

/* data ready interrupt
 */
ISR(PCINT0_vect) {
	drdy = true;
}

void gyroInit () {
	/* set PB1 to input, with pull-up */
	DDRB = (DDRB & ~((1 << PB1)));
	PORTB = (PORTB | (1 << PB1));
	/* enable interrupt PCI0 */
	PCICR = (1 << PCIE0);
	/* enable interrupts on PB1/PCINT1 */
	PCMSK0 = (1 << 1);
}

/* XXX: make nonblocking */
void gyroStart () {
	/* configuration:
	 * disable power-down-mode
	 * defaults
	 * enable drdy interrupt
	 * select 500dps
	 */
	uint8_t data[] = {0b00001111, 0b0, 0b00001000, 0b00010000};

	if (!twRequest (TWM_WRITE, L3GD20, L3GD20_CTRLREG1, data,
			sizeof (data)/sizeof (*data))) {
		printf ("cannot start write\n");
	}
	sleepwhile (twr.status == TWST_WAIT);
	printf ("final twi status was %i\n", twr.status);
}

/*	process gyro sensor data, returns true if new data is available
 */
bool gyroProcess () {
	if (reading) {
		if (twr.status == TWST_OK) {
			/* new data transfered, process it */
			for (uint8_t i = 0; i < sizeof (angle)/sizeof (*angle); i++) {
				angle[i] += (val[i] >> 5) * GYRO_MANGLE_PER_32;
			}
			reading = false;
			return true;
		} else if (twr.status == TWST_ERR) {
			printf ("gyro i2c error\n");
			reading = false;
		}
	} else {
		if (drdy && twr.status != TWST_WAIT) {
			/* new data available in device buffer and bus is free */
			if (!twRequest (TWM_READ, L3GD20, 0x28, (uint8_t *) val, 6)) {
				printf ("cannot start read\n");
			} else {
				drdy = false;
				reading = true;
			}
		}
	}

	return false;
}

const int16_t *gyroGetAngle () {
	return angle;
}

void gyroResetAngle () {
	memset (angle, 0, sizeof (angle));
}

volatile const int16_t *gyroGetRaw () {
	return val;
}

