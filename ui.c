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
/* 10 seconds */
#define SELECT_M1_INC 10
/* 1 minute steps for mode 2 */
#define SELECT_M2_INC 60
/* start of mode 2, seconds */
#define SELECT_M2_START (5*60)
#define SELECT_MAX (60*60)
/* stop alarm after #seconds */
#define ALARM_TIME 30

typedef enum {
	/* initialize */
	UIMODE_INIT,
	/* deep sleep */
	UIMODE_SLEEP,
	/* select time */
	UIMODE_SELECT,
	/* idle, showing time */
	UIMODE_IDLE,
	/* count time */
	UIMODE_RUN,
	/* alert */
	UIMODE_ALARM,
} uimode;

static uimode mode = UIMODE_INIT;
/* timer seconds */
static int16_t seconds = 0;
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

/*
 *	We use the following LED coding (top to bottom):
 *	1-5 minutes:  (off) (5m ) (4m ) (3m ) (2m ) (1m )
 *	5-60 minutes: (on ) (50m) (40m) (30m) (20m) (10m)
 */
static void updateLeds () {
	if (seconds <= SELECT_M2_START) {
		const uint8_t minutes = seconds / 60;
		for (uint8_t i = 0; i < minutes; i++) {
			pwmSetBlink (horizonLed (i), PWM_BLINK_ON);
		}
		/* 10 second steps */
		pwmSetBlink (horizonLed (minutes), (seconds - minutes*60)/10);
		for (uint8_t i = minutes+1; i < PWM_LED_COUNT; i++) {
			pwmSetBlink (horizonLed (i), PWM_BLINK_OFF);
		}
	} else {
		const uint8_t tenminutes = seconds/60/10;
		for (uint8_t i = 0; i < tenminutes; i++) {
			pwmSetBlink (horizonLed (i), PWM_BLINK_ON);
		}
		/* 2 minute steps */
		pwmSetBlink (horizonLed (tenminutes),
				(seconds - tenminutes*10*60)/60/2);
		for (uint8_t i = tenminutes+1; i < PWM_LED_COUNT-1; i++) {
			pwmSetBlink (horizonLed (i), PWM_BLINK_OFF);
		}
		pwmSetBlink (horizonLed (PWM_LED_COUNT-1), PWM_BLINK_ON);
	}
}

/*	Timer value selection
 */
static void doSelect () {
	if (accelGetShakeCount () >= 2) {
		/* stop selection */
		accelResetShakeCount ();
		mode = UIMODE_IDLE;
		printf ("select->idle(%i)\n", seconds);
		speakerStart ();
		_delay_ms (50);
		speakerStop ();
		return;
	}

	/* use zticks as seconds */
	const int16_t zticks = gyroGetZTicks ();
	if (abs (zticks) > 0) {
		gyroResetZTicks ();
		if (seconds > SELECT_M2_START) {
			const int16_t newseconds = seconds + sign (zticks) * SELECT_M2_INC;
			/* when decrementing one minute steps might be too much */
			if (newseconds < SELECT_M2_START) {
				seconds = SELECT_M2_START;
			} else {
				seconds = newseconds;
			}
		} else {
			/* 10 second steps */
			seconds += sign (zticks) * SELECT_M1_INC;
		}
		if (seconds < 0) {
			seconds = 0;
		} else if (seconds > SELECT_MAX) {
			seconds = SELECT_MAX;
		}
		printf ("%i\n", seconds);
	}

	updateLeds ();
}

/*	Idle function, waits for timer start or select commands
 */
static void doIdle () {
	if (horizonChanged && seconds > 0) {
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
		mode = UIMODE_SELECT;
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
		--seconds;
		printf ("run: %i\n", seconds);
		updateLeds ();
		if (seconds == 0) {
			/* blink all leds */
			for (uint8_t i = 0; i < PWM_LED_COUNT; i++) {
				pwmSetBlink (i, 1);
			}
			seconds = ALARM_TIME;
			mode = UIMODE_ALARM;
			printf ("run->alarm\n");
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
		--seconds;
	}
	if (horizonChanged || seconds == 0) {
		timerStop ();
		seconds = 0;
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
		updateLeds ();
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

			case UIMODE_SELECT:
				doSelect ();
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

