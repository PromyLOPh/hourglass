#include "common.h"

#include <util/delay.h>
#include <avr/sleep.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

#include "ui.h"
#include "accel.h"
#include "gyro.h"
#include "speaker.h"
#include "timer.h"
#include "pwm.h"

#define sign(x) ((x < 0) ? -1 : 1)
/* stop alarm after #seconds */
#define ALARM_TIME 30

typedef enum {
	/* initialize */
	UIMODE_INIT,
	/* deep sleep */
	UIMODE_SLEEP,
	/* select time */
	UIMODE_SELECT_COARSE,
	UIMODE_SELECT_FINE,
	/* idle, showing time */
	UIMODE_IDLE,
	/* count time */
	UIMODE_RUN,
	/* alert */
	UIMODE_ALARM,
} uimode;

static uimode mode = UIMODE_INIT;
/* timer seconds */
static int16_t coarseSeconds = 0, fineSeconds = 0;
static uint8_t step = 0, substep = 0;
static uint16_t secPerSubstep = 0;
static uint16_t substepsec = 0;
static horizon h = HORIZON_NONE;
static bool horizonChanged = false;

/*	Read sensor values
 */
static void processSensors () {
	static bool checkGyro = false;

	/* round-robin to prevent starvation */
	if (checkGyro) {
		gyroProcess ();
		accelProcess();
	} else {
		accelProcess ();
		gyroProcess ();
	}
	checkGyro = !checkGyro;
}

/*	Translate LED ids based on horizon, id 0 is always at the bottom of the
 *	device */
static uint8_t horizonLed (uint8_t i) {
	if (h == HORIZON_NEG) {
		return i;
	} else {
		return (PWM_LED_COUNT-1)-i;
	}
}

static int16_t limits (const int16_t in, const int16_t min, const int16_t max) {
	if (in < min) {
		return min;
	} else if (in > max) {
		return max;
	} else {
		return in;
	}
}

/*	Timer value selection
 */
static void doSelectCoarse () {
	if (accelGetShakeCount () >= 2) {
		/* stop selection */
		accelResetShakeCount ();
		mode = UIMODE_SELECT_FINE;
		printf ("selectcoarse->selectfine(%i)\n", coarseSeconds);
		return;
	}

	/* use zticks as seconds */
	const int16_t zticks = gyroGetZTicks ();
	if (abs (zticks) > 0) {
		gyroResetZTicks ();
		coarseSeconds = limits(coarseSeconds + zticks*60*5, 0, 60*60);
		printf ("c:%it:%i\n", coarseSeconds, zticks);

		const uint8_t tenminutes = coarseSeconds/60/10;
		for (uint8_t i = 0; i < tenminutes; i++) {
			pwmSetBlink (horizonLed (i), PWM_BLINK_ON);
		}
		for (uint8_t i = tenminutes; i < PWM_LED_COUNT; i++) {
			pwmSetBlink (horizonLed (i), PWM_BLINK_OFF);
		}
	}
}

static void doSelectFine () {
	if (accelGetShakeCount () >= 2) {
		/* stop selection */
		accelResetShakeCount ();
		step = 6;
		substep = 3;
		secPerSubstep = (coarseSeconds + fineSeconds)/(6*3);
		mode = UIMODE_IDLE;
		printf ("selectfine->idle(%u,%u)\n", coarseSeconds + fineSeconds, secPerSubstep);
		return;
	}

	/* use zticks as seconds */
	const int16_t zticks = gyroGetZTicks ();
	if (abs (zticks) > 0) {
		gyroResetZTicks ();
		fineSeconds = limits(fineSeconds + zticks*30, -5*60, 5*60);
		printf ("f:%it:%i\n", fineSeconds, zticks);

		const uint8_t minutes = abs (fineSeconds)/60;
		for (uint8_t i = 0; i < minutes; i++) {
			pwmSetBlink (horizonLed (i), PWM_BLINK_ON);
		}
		for (uint8_t i = minutes; i < PWM_LED_COUNT-1; i++) {
			pwmSetBlink (horizonLed (i), PWM_BLINK_OFF);
		}
		if (fineSeconds < 0) {
			pwmSetBlink (horizonLed (PWM_LED_COUNT-1), PWM_BLINK_ON);
		} else {
			pwmSetBlink (horizonLed (PWM_LED_COUNT-1), PWM_BLINK_OFF);
		}
	}
}

/*	Idle function, waits for timer start or select commands
 */
static void doIdle () {
	if (horizonChanged) {
		/* start timer */
		mode = UIMODE_RUN;
		timerStart ();
		printf ("idle->run\n");
		speakerStart ();
		_delay_ms (50);
		speakerStop ();
	} else if (accelGetShakeCount () >= 2) {
		/* set timer */
		accelResetShakeCount ();
		mode = UIMODE_SELECT_COARSE;
		printf ("idle->select\n");
		speakerStart ();
		_delay_ms (50);
		speakerStop ();
		return;
	}
}

/*	Run timer, alarm when count==0 or abort when horizon changed
 */
static void doRun () {
	if (timerHit ()) {
		++substepsec;
		if (substepsec > secPerSubstep) {
			--substep;
			substepsec = 0;
		}
		if (substep == 0) {
			--step;
			substep = 3;
			substepsec = 0;
		}
		printf("s:%uss:%u\n", step, substep);
		if (step == 0) {
			/* blink all leds */
			for (uint8_t i = 0; i < PWM_LED_COUNT; i++) {
				pwmSetBlink (i, 1);
			}
			step = ALARM_TIME;
			mode = UIMODE_ALARM;
			printf ("run->alarm\n");
		} else {
			for (uint8_t i = 0; i < step-1; i++) {
				pwmSetBlink (horizonLed (i), PWM_BLINK_ON);
			}
			pwmSetBlink (horizonLed (step-1), PWM_BLINK_ON);
			for (uint8_t i = step; i < PWM_LED_COUNT; i++) {
				pwmSetBlink (horizonLed (i), PWM_BLINK_OFF);
			}
		}
	} else if (horizonChanged) {
		/* stop timer */
		mode = UIMODE_IDLE;
		printf ("run->idle (stopped)\n");
	}
}

/*	Run alarm for some time or user interaction, then stop
 */
static void doAlarm () {
	if (timerHit ()) {
		--step;
	}
	if (horizonChanged || step == 0) {
		timerStop ();
		step = 0;
		/* stop blinking */
		for (uint8_t i = 0; i < PWM_LED_COUNT; i++) {
			pwmSetBlink (i, PWM_BLINK_OFF);
		}
		mode = UIMODE_IDLE;
	}
}

/*	Wait for sensor initialization
 */
static void doInit () {
	/* get initial orientation */
	h = accelGetHorizon ();
	if (h != HORIZON_NONE) {
		mode = UIMODE_IDLE;
		printf ("init->idle\n");
		for (uint8_t i = 0; i < PWM_LED_COUNT; i++) {
			pwmSetBlink (horizonLed (i), PWM_BLINK_OFF);
		}
	}
}

/*	Sleep CPU
 */
static void cpuSleep () {
	sleep_enable ();
	sleep_cpu ();
	sleep_disable ();
}

/*	Main loop
 */
void uiLoop () {
	while (1) {
		processSensors ();
		
		horizon newh = accelGetHorizon ();
		if (newh != h) {
			horizonChanged = true;
		} else {
			horizonChanged = false;
		}
		h = newh;

		switch (mode) {
			case UIMODE_INIT:
				doInit ();
				break;

			case UIMODE_SELECT_COARSE:
				doSelectCoarse ();
				break;

			case UIMODE_SELECT_FINE:
				doSelectFine ();
				break;

			case UIMODE_IDLE:
				doIdle ();
				break;

			case UIMODE_RUN:
				doRun ();
				break;

			case UIMODE_ALARM:
				doAlarm ();
				break;

			default:
				assert (0 && "invalid ui mode");
				break;
		}
		cpuSleep ();

#if 0
		printf ("t=%i, h=%i, s=%i\n", gyroGetZTicks (), accelGetHorizon (),
				accelGetShakeCount ());
		volatile const int32_t *gyroval = gyroGetAccum ();
		volatile const int16_t *gyroraw = gyroGetRaw ();
		volatile const int8_t *accelval = accelGet ();
		printf ("%li/%li/%li - %i/%i/%i - %i/%i/%i\n",
				gyroval[0], gyroval[1], gyroval[2],
				gyroraw[0], gyroraw[1], gyroraw[2],
				accelval[1], accelval[3], accelval[5]);
#endif
	}
}

