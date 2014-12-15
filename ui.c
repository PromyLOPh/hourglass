#include "common.h"

#include <util/delay.h>
#include <avr/sleep.h>
#include <stdio.h>
#include <stdlib.h>

#include "ui.h"
#include "accel.h"
#include "gyro.h"
#include "timer.h"
#include "pwm.h"

#define sign(x) ((x < 0) ? -1 : 1)
/* keep the lights on for 10 ms */
#define ALARM_TIME_FLASH ((uint32_t) 10*1000)
/* and wait 500 ms */
#define ALARM_TIME_WAIT ((uint32_t) 500*1000)
/* flash 30 times */
#define ALARM_FLASHES 30

typedef enum {
	/* initialize */
	UIMODE_INIT,
	/* deep sleep */
	UIMODE_SLEEP,
	/* select time */
	UIMODE_SELECT_COARSE,
	UIMODE_SELECT_FINE,
	/* idle */
	UIMODE_IDLE,
	/* count time */
	UIMODE_RUN,
	/* alert */
	UIMODE_ALARM_FLASH,
	UIMODE_ALARM_WAIT,
} uimode;

static uimode mode = UIMODE_INIT;
/* Selection values */
static signed char coarseValue = 0, fineValue = 0;
/* timer seconds (us) */
static uint32_t timerElapsed = 0, timerValue;
static uint8_t brightness[PWM_LED_COUNT];
static uint8_t currLed;
static horizon h = HORIZON_NONE;
static bool horizonChanged = false;
static uint8_t alarmFlashes = 0;

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

/*	Enter idle mode
 */
static void enterIdle () {
	mode = UIMODE_IDLE;

	pwmSet (horizonLed (0), 1);
	for (uint8_t i = 1; i < PWM_LED_COUNT; i++) {
		pwmSet (horizonLed (i), PWM_OFF);
	}
}

static void enterAlarmFlash () {
	mode = UIMODE_ALARM_FLASH;
	for (uint8_t i = 0; i < PWM_LED_COUNT; i++) {
		pwmSet (i, PWM_ON);
	}
	timerStart (ALARM_TIME_FLASH);
}

/*	Set value from fine selection and show with leds
 */
static void setFine (const int8_t value) {
	/* min timer value is 1 minute, disable subtract if coarse is below ten
	 * minutes */
	const int8_t bottomlimit = coarseValue == 0 ? 1 : -5;
	fineValue = limits(value, bottomlimit, 5);
	puts ("\nfineValue\n");
	fwrite (&fineValue, sizeof (fineValue), 1, stdout);

	/* from bottom to top for positive values, top to bottom for negative
	 * values */
	if (fineValue >= 0) {
		for (uint8_t i = 0; i < fineValue; i++) {
			pwmSet (horizonLed (i), PWM_ON);
		}
		for (uint8_t i = fineValue; i < PWM_LED_COUNT; i++) {
			pwmSet (horizonLed (i), PWM_OFF);
		}
	} else {
		for (uint8_t i = 0; i < abs (fineValue); i++) {
			pwmSet (horizonLed (PWM_LED_COUNT-1-i), PWM_ON);
		}
		for (uint8_t i = abs (fineValue); i < PWM_LED_COUNT; i++) {
			pwmSet (horizonLed (PWM_LED_COUNT-1-i), PWM_OFF);
		}
	}
}

/*	Coarse timer setting, selects from 0 to 60 minutes, in 10 min steps
 */
static void doSelectCoarse () {
	if (accelGetShakeCount () >= 2) {
		/* selection */
		accelResetShakeCount ();
		mode = UIMODE_SELECT_FINE;
		puts ("selectcoarse->selectfine");
		setFine (0);
		speakerStart (SPEAKER_BEEP);
		return;
	}

	/* use zticks as seconds */
	const int16_t zticks = gyroGetZTicks ();
	if (abs (zticks) > 0) {
		gyroResetZTicks ();
		coarseValue = limits(coarseValue + zticks, 0, 6);
		/* at least 1 min */
		fineValue = coarseValue == 0 ? 1 : 0;
		puts ("\ncoarseValue\n");
		fwrite (&coarseValue, sizeof (coarseValue), 1, stdout);

		for (uint8_t i = 0; i < coarseValue; i++) {
			pwmSet (horizonLed (i), PWM_ON);
		}
		for (uint8_t i = coarseValue; i < PWM_LED_COUNT; i++) {
			pwmSet (horizonLed (i), PWM_OFF);
		}
	}
}

/*	Fine timer setting, selects from -5 to 5 minutes, in 1 min steps
 */
static void doSelectFine () {
	if (accelGetShakeCount () >= 2) {
		/* stop selection */
		accelResetShakeCount ();
		speakerStart (SPEAKER_BEEP);
		gyroStop ();

		puts ("selectfine->idle");
		enterIdle ();
		return;
	}

	/* use zticks as seconds */
	const int16_t zticks = gyroGetZTicks ();
	if (abs (zticks) > 0) {
		gyroResetZTicks ();
		setFine (fineValue + zticks);
	}
}

/*	Idle function, waits for timer start or select commands
 */
static void doIdle () {
	if (horizonChanged) {
		/* start timer */
		for (uint8_t i = 0; i < PWM_LED_COUNT; i++) {
			brightness[i] = 0;
			pwmSet (horizonLed (i), PWM_OFF);
		}
		currLed = PWM_LED_COUNT-1;
		brightness[currLed] = PWM_MAX_BRIGHTNESS;
		pwmSet (horizonLed (currLed), brightness[currLed]);

		timerValue = coarseValue * (uint32_t) 10*60*1000*1000 +
				fineValue * (uint32_t) 60*1000*1000;
		puts ("\ntimerValue\n");
		fwrite (&timerValue, sizeof (timerValue), 1, stdout);
		timerElapsed = 0;
		/* (PWM_LED_COUNT-1)*PWM_MAX_BRIGHTNESS states; -1, since two leds’s
		 * states are interleaved */
		const uint32_t brightnessStep = timerValue/(uint32_t) ((PWM_LED_COUNT-1)*PWM_MAX_BRIGHTNESS);
		puts ("\nbrightnessStep\n");
		fwrite (&brightnessStep, sizeof (brightnessStep), 1, stdout);

		mode = UIMODE_RUN;
		timerStart (brightnessStep);
		puts ("idle->run");
		speakerStart (SPEAKER_BEEP);
	} else if (accelGetShakeCount () >= 2) {
		/* set timer */
		accelResetShakeCount ();
		gyroStart ();
		mode = UIMODE_SELECT_COARSE;
		puts ("idle->select");
		speakerStart (SPEAKER_BEEP);
		coarseValue = 0;
		return;
	}
}

/*	Run timer, alarm when count==0 or abort when horizon changed
 */
static void doRun () {
	const uint32_t t = timerHit ();
	if (t > 0) {
		timerElapsed += t;
		puts ("\ntimerElapsed");
		fwrite (&timerElapsed, sizeof (timerElapsed), 1, stdout);
		if (timerElapsed >= timerValue) {
			/* ring the alarm! */
			for (uint8_t i = 0; i < PWM_LED_COUNT; i++) {
				pwmSet (i, PWM_MAX_BRIGHTNESS);
			}
			timerStop ();
			puts ("run->alarm");
			/* beep only once */
			speakerStart (SPEAKER_BEEP);
			alarmFlashes = ALARM_FLASHES;
			enterAlarmFlash ();
		} else if (currLed > 0) {
			/* one step */
			--brightness[currLed];
			pwmSet (horizonLed (currLed), brightness[currLed]);
			++brightness[currLed-1];
			pwmSet (horizonLed (currLed-1), brightness[currLed-1]);
			if (brightness[currLed] == 0 && currLed > 0) {
				--currLed;
			}
			puts ("\ncurrLed");
			fwrite (&currLed, sizeof (currLed), 1, stdout);
			puts ("\nbrightness");
			fwrite (&brightness, sizeof (brightness), 1, stdout);
		}
	} else if (horizonChanged) {
		/* stop timer */
		speakerStart (SPEAKER_BEEP);

		puts ("run->idle (stopped)");
		enterIdle ();
	}
}

/*	LEDs are currently on, waiting for horizon change (which stops the alarm)
 *	or next wait period
 */
static void doAlarmFlash () {
	const uint32_t t = timerHit ();
	if (horizonChanged) {
		puts ("alarm->idle");
		enterIdle ();
	} else if (t > 0) {
		puts ("alarmflash->alarmwait");
		mode = UIMODE_ALARM_WAIT;
		timerStop ();
		for (uint8_t i = 0; i < PWM_LED_COUNT; i++) {
			pwmSet (i, PWM_OFF);
		}
		timerStart (ALARM_TIME_WAIT);
	}
}

/*	LEDs are currently off, waiting for horizon change (which stops the alarm)
 *	or the next flash period
 */
static void doAlarmWait () {
	const uint32_t t = timerHit ();
	if (horizonChanged) {
		puts ("alarmwait->idle");
		enterIdle ();
	} else if (t > 0) {
		if (--alarmFlashes == 0) {
			puts ("alarmwait->idle");
			enterIdle ();
		} else {
			puts ("alarmwait->alarmflash");
			enterAlarmFlash ();
		}
	}
}

/*	Wait for sensor initialization
 */
static void doInit () {
	/* get initial orientation */
	if (horizonChanged && h != HORIZON_NONE) {
		puts ("init->idle");
		enterIdle ();

#if 0
		/* debugging */
		mode = UIMODE_RUN;
		timerValue = (uint32_t) 60*1000*1000;
		timerElapsed = 0;
		/* (PWM_LED_COUNT-1)*PWM_MAX_BRIGHTNESS states; -1, since two leds’s
		 * states are interleaved */
		brightnessStep = timerValue/(uint32_t) ((PWM_LED_COUNT-1)*PWM_MAX_BRIGHTNESS);
		puts ("\nbrightnessStep\n");
		fwrite (&brightnessStep, sizeof (brightnessStep), 1, stdout);

		currLed = PWM_LED_COUNT-1;
		brightness[currLed] = PWM_MAX_BRIGHTNESS;
		pwmSet (horizonLed (currLed), brightness[currLed]);
		timerStart (brightnessStep);
#endif
	}
}

/*	Main loop
 */
void uiLoop () {
#if 0
	/* LED test mode */
	uint8_t i = 0;
	uint8_t brightness = 0;
	while (1) {
		for (uint8_t j = 0; j < PWM_LED_COUNT; j++) {
			pwmSet (horizonLed (j), PWM_OFF);
		}
		pwmSet (horizonLed (i), brightness);
		++i;
		if (i >= PWM_LED_COUNT) {
			i = 0;
			++brightness;
			if (brightness > PWM_MAX_BRIGHTNESS) {
				brightness = 0;
			}
		}
		_delay_ms (1000);
	}
#endif

#if 0
	/* timer test mode */
	timerStart ((uint32_t) 10*1000*1000);
	while (1) {
		uint32_t t;
		while (1) {
			t = timerHit ();
			if (t > 0) {
				break;
			}
			cpuSleep ();
		}
		puts ("on");
		fwrite (&t, sizeof (t), 1, stdout);
		pwmSet (horizonLed (0), PWM_ON);

		while (1) {
			t = timerHit ();
			if (t > 0) {
				break;
			}
			cpuSleep ();
		}
		puts ("off");
		fwrite (&t, sizeof (t), 1, stdout);
		pwmSet (horizonLed (0), PWM_OFF);
	}
#endif

	/* startup, test all LED’s */
	pwmStart ();
	for (uint8_t i = 0; i < PWM_LED_COUNT; i++) {
		pwmSet (i, PWM_ON);
	}
	pwmSet (0, PWM_OFF);
	accelStart ();
	pwmSet (1, PWM_OFF);

	/* make sure data is read even when missing the first interrupt (i.e.
	 * reboot) */
	enableWakeup (WAKE_ACCEL);
	enableWakeup (WAKE_GYRO);

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

			case UIMODE_ALARM_FLASH:
				doAlarmFlash ();
				break;

			case UIMODE_ALARM_WAIT:
				doAlarmWait ();
				break;

			default:
				assert (0 && "invalid ui mode");
				break;
		}

		sleepwhile (wakeup == 0);

#if 0
		printf ("t=%i, h=%i, s=%i\n", gyroGetZTicks (), accelGetHorizon (),
				accelGetShakeCount ());
		const int32_t gyroval = gyroGetZAccum ();
		const int16_t gyroraw = gyroGetZRaw ();
		const int8_t accelval = accelGetZ ();
		printf ("%li - %i - %i\n", gyroval, gyroraw, accelval);
#endif
	}

	timerStop ();
	pwmStop ();
}

