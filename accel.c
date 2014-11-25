#include "common.h"

#include <stdio.h>
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
#define ACCEL_1G_POS (55)
#define ACCEL_1G_NEG (-55)
/* offset for horizon detection */
#define ACCEL_1G_OFF (5)
/* shake detection values; 2g for +-2g max */
#define ACCEL_SHAKE_POS (INT8_MAX)
#define ACCEL_SHAKE_NEG (INT8_MIN)
/* 250ms for 100Hz data rate */
#define ACCEL_SHAKE_TIMEOUT (25)

/* z value */
static volatile int8_t zval = 0;
/* number of times shaken (i.e. peak level measured) */
static uint8_t shakeCount = 0;
/* if max in one direction direction has been detected give it some time to
 * wait for max in the other direction */
static uint8_t shakeTimeout = 0;
/* sign of last shake peak */
static enum {SHAKE_NONE, SHAKE_POS, SHAKE_NEG} shakeSign = SHAKE_NONE;
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
		puts ("cannot start write");
	}
	sleepwhile (twr.status == TWST_WAIT);
	assert (twr.status == TWST_OK);
	puts ("accelStart done");
	disableWakeup (WAKE_I2C);
}

/*	register shake gesture
 *
 *	“shake” means a peak in one direction followed by another one in the other
 *	direction. called for every data set pulled.
 */
static void accelProcessShake () {
	/* detect shake if:
	 * a) horizon is positive and accel z value is >= ACCEL_SHAKE_POS
	 * b) horizon is negative and accel z value is >= ACCEL_SHAKE_POS offset by
	 *    the value for 1g (negative)
	 * (same for negative horizon)
	 */
	if (((zval >= ACCEL_SHAKE_POS &&
			horizonSign == HORIZON_POS) ||
			(zval >= (ACCEL_SHAKE_POS + ACCEL_1G_NEG) &&
			horizonSign == HORIZON_NEG)) &&
			shakeSign != SHAKE_POS) {
		/* if we did not time out (i.e. max in other direction has been
		 * detected) register shake */
		if (shakeTimeout > 0) {
			++shakeCount;
			/* correctly detect double/triple/… shakes; setting this to
			 * ACCEL_SHAKE_TIMEOUT yields wrong results */
			shakeTimeout = 0;
		} else {
			shakeTimeout = ACCEL_SHAKE_TIMEOUT;
		}
		shakeSign = SHAKE_POS;
	} else if (((zval <= ACCEL_SHAKE_NEG &&
			horizonSign == HORIZON_NEG) ||
			(zval <= (ACCEL_SHAKE_NEG + ACCEL_1G_POS) &&
			horizonSign == HORIZON_POS)) &&
			shakeSign != SHAKE_NEG) {
		if (shakeTimeout > 0) {
			++shakeCount;
			shakeTimeout = 0;
		} else {
			shakeTimeout = ACCEL_SHAKE_TIMEOUT;
		}
		shakeSign = SHAKE_NEG;
	} else {
		if (shakeTimeout > 0) {
			--shakeTimeout;
		}
	}
}

/*	register horizon change
 *
 *	i.e. have we been turned upside down?
 */
static void accelProcessHorizon () {
	/* measuring approximately 1g */
	if (zval > (ACCEL_1G_POS - ACCEL_1G_OFF) &&
			zval < (ACCEL_1G_POS + ACCEL_1G_OFF) &&
			horizonPrevSign == HORIZON_POS && horizonSign != HORIZON_POS) {
		++horizonStable;
	} else if (zval < (ACCEL_1G_NEG + ACCEL_1G_OFF)
			&& zval > (ACCEL_1G_NEG - ACCEL_1G_OFF) &&
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
			accelProcessShake ();
			/* new data transfered */
			return true;
		} else if (twr.status == TWST_ERR) {
			puts ("accel i2c error: ");
			fwrite ((void *) &twr.error, sizeof (twr.error), 1, stdout);
		}
	} else {
		if (shouldWakeup (WAKE_ACCEL) && twr.status == TWST_OK) {
			/* new data available in device buffer and bus is free */
			if (!twRequest (TWM_READ, LIS302DL, LIS302DL_OUTZ,
					(uint8_t *) &zval, sizeof (zval))) {
				puts ("cannot start read");
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

uint8_t accelGetShakeCount () {
	return shakeCount;
}

void accelResetShakeCount () {
	shakeCount = 0;
}

horizon accelGetHorizon () {
	return horizonSign;
}

