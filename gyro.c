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
static enum {STOPPED = 0, STARTING, STOPPING, READING, IDLE} state = STOPPED;

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
	/* configuration:
	 * disable power-down-mode, enable z
	 * defaults
	 * high-active, push-pull, drdy on int2
	 * select 2000dps
	 */
	static uint8_t data[] = {0b00001100, 0b0, 0b00001000, 0b00110000};

	assert (state == STOPPED);

	const bool ret = twRequest (TWM_WRITE, L3GD20, L3GD20_CTRLREG1, data,
			sizeof (data)/sizeof (*data));
	assert (ret);
	state = STARTING;
}

void gyroStop () {
	/* enable power-down mode */
	static uint8_t data[] = {0b00000000};

	/* XXX: there might be a race-condition here */
	assert (state == IDLE);

	const bool ret = twRequest (TWM_WRITE, L3GD20, L3GD20_CTRLREG1, data,
			sizeof (data)/sizeof (*data));
	assert (ret);
	state = STOPPING;
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
				assert (twr.status != TWST_ERR);
				if (twr.status == TWST_OK) {
					/* new data transfered, process it */
					/* poor man's noise filter */
					if (abs (zval) > 64) {
						zaccum += zval;
					}
					gyroProcessTicks ();
					return true;
				}
			}
			break;

		case IDLE:
			if (shouldWakeup (WAKE_GYRO) && twr.status == TWST_OK) {
				/* new data available in device buffer and bus is free */
				const bool ret = twRequest (TWM_READ, L3GD20, L3GD20_OUTZ,
						(uint8_t *) &zval, sizeof (zval));
				assert (ret);
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

