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
#define LIS302DL_UNUSED1 0x28
#define LIS302DL_OUTZ 0x2D

/* value for 1g with +-2g max; measured */
#define ACCEL_1G (55)
/* offset for horizon detection */
#define ACCEL_1G_OFF (5)
/* threshold starting shake detection */
#define ACCEL_SHAKE_START (90)
/* difference prev and current value to get the current one registered as peak */
#define ACCEL_SHAKE_REGISTER (120)
/* 100ms for 100Hz data rate */
#define ACCEL_SHAKE_TIMEOUT (10)

/* z value */
static int8_t zval = 0;
static int16_t zvalnormal = 0;

/* number of times shaken (i.e. peak level measured) */
static uint8_t shakeCount = 0;
/* if max in one direction direction has been detected give it some time to
 * wait for max in the other direction */
static uint8_t shakeTimeout = 0;
static int16_t prevShakeVal = 0;
static uint8_t peakCount = 0;
static horizon shakeHorizon = HORIZON_NONE;

/* horizon position */
/* current */
static horizon horizonSign = HORIZON_NONE;
/* how long has sign been stable? */
static uint8_t horizonStable = 0;
/* previous measurement */
static horizon horizonPrevSign = HORIZON_NONE;

/* currently reading from i2c */
static bool reading = false;

/* data ready interrupt
 */
ISR(PCINT1_vect) {
	const bool interrupt = (PINC >> PINC1) & 0x1;
	/* high-active */
	if (interrupt) {
		enableWakeup (WAKE_ACCEL);
	} else {
		disableWakeup (WAKE_ACCEL);
	}
}

void accelInit () {
	/* set interrupt lines to input */
	DDRC = DDRC & ~((1 << DDC0) | (1 << DDC1));
	/* enable interrupt PCI1 for PCINT8/9 */
	PCICR = PCICR | (1 << PCIE1);
	/* enable interrupts from port PC0/PC1 aka PCINT8/PCINT9 */
	PCMSK1 = (1 << PCINT9) | (1 << PCINT8);
}

/* XXX: make nonblocking */
void accelStart () {
	/* configuration:
	 * disable power-down-mode, enable z-axis
	 * defaults
	 * push-pull, high-active, data ready interrupt on int2
	 */
	uint8_t data[] = {0b01000100, 0b0, 0b00100000};

	if (!twRequest (TWM_WRITE, LIS302DL, LIS302DL_CTRLREG1, data,
			sizeof (data)/sizeof (*data))) {
		assert (0);
	}
	sleepwhile (twr.status == TWST_WAIT);
	assert (twr.status == TWST_OK);
	disableWakeup (WAKE_I2C);
}

/*	register shake gesture
 *
 *	“shake” means three peaks in directions 1, -1, 1
 */
static void accelProcessShake () {
	if (shakeTimeout > 0) {
		--shakeTimeout;
		if (horizonSign != shakeHorizon) {
			/* ignore if horizon changed */
			shakeTimeout = 0;
			prevShakeVal = 0;
			peakCount = 0;
		} else if (sign (prevShakeVal) != sign (zvalnormal) &&
				abs (prevShakeVal - zvalnormal) >= ACCEL_SHAKE_REGISTER) {
			++peakCount;
			shakeTimeout = ACCEL_SHAKE_TIMEOUT;
			prevShakeVal = zvalnormal;
		} else if (sign (prevShakeVal) == sign (zvalnormal) &&
				abs (zvalnormal) >= abs (prevShakeVal)) {
			/* actually we did not measure the peak yet/are still experiencing it */
			prevShakeVal = zvalnormal;
			shakeTimeout = ACCEL_SHAKE_TIMEOUT;
		}
		if (shakeTimeout == 0) {
			/* just timed out, can register gesture now */
			shakeCount += peakCount/2;
			prevShakeVal = 0;
			peakCount = 0;
		}
	}

	/* start shake detection */
	if (shakeTimeout == 0 && abs (zvalnormal) >= ACCEL_SHAKE_START) {
		shakeTimeout = ACCEL_SHAKE_TIMEOUT;
		peakCount = 1;
		prevShakeVal = zvalnormal;
		shakeHorizon = horizonSign;
	}
}

/*	register horizon change
 *
 *	i.e. have we been turned upside down?
 */
static void accelProcessHorizon () {
	/* measuring approximately 1g */
	if (zval > (ACCEL_1G - ACCEL_1G_OFF) &&
			zval < (ACCEL_1G + ACCEL_1G_OFF) &&
			horizonPrevSign == HORIZON_POS && horizonSign != HORIZON_POS) {
		++horizonStable;
	} else if (zval < (-ACCEL_1G + ACCEL_1G_OFF)
			&& zval > (-ACCEL_1G - ACCEL_1G_OFF) &&
			horizonPrevSign == HORIZON_NEG && horizonSign != HORIZON_NEG) {
		++horizonStable;
	} else {
		horizonStable = 0;
	}
	/* make sure its not just shaking */
	if (horizonStable > 5) {
		horizonSign = horizonPrevSign;
		horizonStable = 0;
	}
	horizonPrevSign = zval >= 0 ? HORIZON_POS : HORIZON_NEG;
}

bool accelProcess () {
	if (reading && shouldWakeup (WAKE_I2C)) {
		disableWakeup (WAKE_I2C);
		reading = false;
		if (twr.status == TWST_OK) {
			accelProcessHorizon ();

			/* calculate normalized z (i.e. without earth gravity component) */
			if (horizonSign == HORIZON_NEG) {
				zvalnormal = zval - (-ACCEL_1G);
			} else if (horizonSign == HORIZON_POS) {
				zvalnormal = zval - ACCEL_1G;
			}

			accelProcessShake ();
			/* new data transfered */
			return true;
		} else if (twr.status == TWST_ERR) {
			assert (0);
		}
	} else {
		if (shouldWakeup (WAKE_ACCEL) && twr.status == TWST_OK) {
			/* new data available in device buffer and bus is free */
			if (!twRequest (TWM_READ, LIS302DL, LIS302DL_OUTZ,
					(uint8_t *) &zval, sizeof (zval))) {
				assert (0);
			} else {
				/* wakeup source is disabled by isr to prevent race condition */
				reading = true;
			}
		}
	}

	return false;
}

int8_t accelGetZ () {
	return zval;
}

int8_t accelGetNormalizedZ () {
	return zvalnormal;
}

uint8_t accelGetShakeCount () {
	return shakeCount;
}

void accelResetShakeCount () {
	shakeCount = 0;
}

horizon accelGetHorizon () {
	return horizonSign;
}

