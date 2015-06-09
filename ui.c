/*
Copyright (c) 2014-2015
	Lars-Dominik Braun <lars@6xq.net>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

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

/* keep the lights on for 10 ms */
#define FLASH_ALARM_ON ((uint32_t) 10*1000)
/* and wait 500 ms */
#define FLASH_ALARM_OFF ((uint32_t) 500*1000)
/* flash 30 times */
#define FLASH_ALARM_NUM (30)

#define FLASH_ENTER_COARSE_ON ((uint32_t) 50*1000)
/* time is ignored for ENTER/CONFIRM_COARSE (because _NUM is one), but used for
 * CONFIRM_FINE */
#define FLASH_ENTER_COARSE_OFF ((uint32_t) 200*1000)
#define FLASH_ENTER_COARSE_NUM (1)

#define FLASH_CONFIRM_COARSE_ON FLASH_ENTER_COARSE_ON
#define FLASH_CONFIRM_COARSE_OFF FLASH_ENTER_COARSE_OFF
#define FLASH_CONFIRM_COARSE_NUM FLASH_ENTER_COARSE_NUM

#define FLASH_CONFIRM_FINE_ON FLASH_ENTER_FINE_ON
#define FLASH_CONFIRM_FINE_OFF FLASH_ENTER_FINE_OFF
#define FLASH_CONFIRM_FINE_NUM (2)

/* UI modes, enum would take 16 bits */
typedef uint8_t uimode;
/* initialize */
#define UIMODE_INIT 0
/* deep sleep */
#define UIMODE_SLEEP 1
/* select time */
#define UIMODE_SELECT_COARSE 2
#define UIMODE_SELECT_FINE 3
/* idle */
#define UIMODE_IDLE 4
/* count time */
#define UIMODE_RUN 5
/* flash leds */
#define UIMODE_FLASH_ON 6
#define UIMODE_FLASH_OFF 7

/* flash modes */
typedef uint8_t flashmode;
#define FLASH_NONE 0
#define FLASH_ALARM 1
#define FLASH_ENTER_COARSE 2
#define FLASH_CONFIRM_COARSE 3
#define FLASH_CONFIRM_FINE 4

/* nextmode is used for deciding which mode _FLASH transitions into */
static uimode mode = UIMODE_INIT;
static flashmode fmode = FLASH_NONE;
static uint8_t flashCount = 0;
/* Temporary persistence while selecting */
static signed char coarseValue = 0, fineValue = 0;
/* Actual elapsed/alarm time in us */
static uint32_t timerElapsed, timerValue;
static uint8_t brightness[PWM_LED_COUNT];
static uint8_t currLed;
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

/*	Enter low-power idle mode
 */
static void enterIdle () {
	mode = UIMODE_IDLE;

	pwmStop ();
	gyroStop ();
}

static void enterFlash (const flashmode next) {
	fmode = next;
	mode = UIMODE_FLASH_ON;
	for (uint8_t i = 0; i < PWM_LED_COUNT; i++) {
		pwmSet (i, PWM_ON);
	}
	switch (fmode) {
		case FLASH_ALARM:
			timerStart (FLASH_ALARM_ON, true);
			break;

		case FLASH_ENTER_COARSE:
		case FLASH_CONFIRM_COARSE:
		case FLASH_CONFIRM_FINE:
			timerStart (FLASH_ENTER_COARSE_ON, true);
			break;

		default:
			assert (0);
			break;
	}
}

static void enterCoarse () {
	gyroStart ();
	mode = UIMODE_SELECT_COARSE;
	speakerStart (SPEAKER_BEEP);
	/* start with a value of zero */
	pwmSetOff ();
	coarseValue = 0;
}

/*	Set value from fine selection and show with leds
 */
static void setFine (const int8_t value) {
	/* min timer value is 1 minute, disable subtract if coarse is below ten
	 * minutes */
	const int8_t bottomlimit = coarseValue == 0 ? 1 : -5;
	fineValue = limits(value, bottomlimit, 5);

	/* from bottom to top for positive values, top to bottom for negative
	 * values */
	if (fineValue >= 0) {
		pwmSetOff ();
		for (uint8_t i = 0; i < fineValue; i++) {
			pwmSet (horizonLed (i), PWM_ON);
		}
	} else {
		pwmSetOff ();
		for (uint8_t i = 0; i < abs (fineValue); i++) {
			pwmSet (horizonLed (PWM_LED_COUNT-1-i), PWM_ON);
		}
	}
}

static void enterFine () {
	/* selection */
	mode = UIMODE_SELECT_FINE;
	setFine (0);
	speakerStart (SPEAKER_BEEP);
}

/*	Coarse timer setting, selects from 0 to 60 minutes, in 10 min steps
 */
static void doSelectCoarse () {
	/* abort without setting value */
	if (horizonChanged) {
		enterIdle ();
		return;
	}

	/* switch to fine selection */
	if (accelGetShakeCount () >= 1) {
		accelResetShakeCount ();
		enterFlash (FLASH_CONFIRM_COARSE);
		return;
	}

	/* use zticks as seconds */
	const int16_t zticks = gyroGetZTicks ();
	if (abs (zticks) > 0) {
		gyroResetZTicks ();
		coarseValue = limits(coarseValue + zticks, 0, 6);
		/* at least 1 min */
		fineValue = coarseValue == 0 ? 1 : 0;

		pwmSetOff ();
		for (uint8_t i = 0; i < coarseValue; i++) {
			pwmSet (horizonLed (i), PWM_ON);
		}
	}
}

/*	Fine timer setting, selects from -5 to 5 minutes, in 1 min steps
 */
static void doSelectFine () {
	/* abort without setting value */
	if (horizonChanged) {
		enterIdle ();
		return;
	}

	if (accelGetShakeCount () >= 1) {
		/* stop selection, actually set timer */
		timerValue = coarseValue * (uint32_t) 10*60*1000*1000 +
				fineValue * (uint32_t) 60*1000*1000;
		accelResetShakeCount ();
		speakerStart (SPEAKER_BEEP);
		gyroStop ();

		enterFlash (FLASH_CONFIRM_FINE);
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
		pwmSetOff ();
		for (uint8_t i = 0; i < PWM_LED_COUNT; i++) {
			brightness[i] = 0;
		}
		currLed = PWM_LED_COUNT-1;
		brightness[currLed] = PWM_MAX_BRIGHTNESS;
		pwmSet (horizonLed (currLed), brightness[currLed]);
		/* (PWM_LED_COUNT-1)*PWM_MAX_BRIGHTNESS states; -1, since two leds’s
		 * states are interleaved */
		const uint32_t brightnessStep = timerValue/(uint32_t) ((PWM_LED_COUNT-1)*PWM_MAX_BRIGHTNESS);

		timerElapsed = 0;

		mode = UIMODE_RUN;
		pwmStart ();
		timerStart (brightnessStep, false);
		speakerStart (SPEAKER_BEEP);
	} else if (accelGetShakeCount () >= 1) {
		/* set timer */
		pwmStart ();
		accelResetShakeCount ();
		enterFlash (FLASH_ENTER_COARSE);
		return;
	}
}

/*	Run timer
 */
static void doRun () {
	/* horizon change is more important than timer, thus checked first */
	if (horizonChanged) {
		/* stop timer */
		speakerStart (SPEAKER_BEEP);

		enterIdle ();
		return;
	}

	const uint32_t t = timerHit ();
	if (t > 0) {
		timerElapsed += t;
		if (timerElapsed >= timerValue) {
			timerStop ();
			/* ring the alarm! */
			speakerStart (SPEAKER_BEEP);
			enterFlash (FLASH_ALARM);
		} else if (currLed > 0) {
			/* one step */
			--brightness[currLed];
			pwmSet (horizonLed (currLed), brightness[currLed]);
			++brightness[currLed-1];
			pwmSet (horizonLed (currLed-1), brightness[currLed-1]);
			if (brightness[currLed] == 0 && currLed > 0) {
				--currLed;
			}
		}
	}
}

/*	LEDs are currently on. Depending on next mode do something like wait for
 *	horizon change (which stops the alarm) or next wait period
 */
static void doFlashOn () {
	bool stopFlash = false;

	switch (fmode) {
		case FLASH_ALARM:
			if (horizonChanged || accelGetShakeCount () > 0) {
				accelResetShakeCount ();
				enterIdle ();
				stopFlash = true;
			}
			break;

		case FLASH_ENTER_COARSE:
		case FLASH_CONFIRM_COARSE:
		case FLASH_CONFIRM_FINE:
			/* pass */
			break;

		default:
			assert (0);
			break;
	}

	if (!stopFlash) {
		const uint32_t t = timerHit ();
		if (t > 0) {
			++flashCount;
			mode = UIMODE_FLASH_OFF;
			pwmSetOff ();
			switch (fmode) {
				case FLASH_ALARM:
					timerStart (FLASH_ALARM_OFF, true);
					break;

				case FLASH_ENTER_COARSE:
				case FLASH_CONFIRM_COARSE:
				case FLASH_CONFIRM_FINE:
					timerStart (FLASH_ENTER_COARSE_OFF, true);
					break;

				default:
					assert (0);
					break;
			}
		}
	}
}

/*	LEDs are currently off, waiting next flash period
 */
static void doFlashOff () {
	bool stopFlash = false;

	switch (fmode) {
		case FLASH_ALARM:
			if (horizonChanged || accelGetShakeCount () > 0 ||
						flashCount >= FLASH_ALARM_NUM) {
				accelResetShakeCount ();
				enterIdle ();
				stopFlash = true;
			}
			break;

		case FLASH_ENTER_COARSE:
			if (flashCount >= FLASH_ENTER_COARSE_NUM) {
				enterCoarse ();
				stopFlash = true;
			}
			break;

		case FLASH_CONFIRM_COARSE:
			if (flashCount >= FLASH_CONFIRM_COARSE_NUM) {
				enterFine ();
				stopFlash = true;
			}
			break;

		case FLASH_CONFIRM_FINE:
			if (flashCount >= FLASH_CONFIRM_FINE_NUM) {
				enterIdle ();
				stopFlash = true;
			}
			break;

		default:
			assert (0);
			break;
	}

	if (!stopFlash) {
		const uint32_t t = timerHit ();
		if (t > 0) {
			enterFlash (fmode);
		}
	} else {
		flashCount = 0;
	}
}

/*	Wait for sensor initialization
 */
static void doInit () {
	/* get initial orientation */
	if (horizonChanged && h != HORIZON_NONE) {
		enterIdle ();

#if 0
		/* debugging */
		mode = UIMODE_RUN;
		timerValue = (uint32_t) 60*1000*1000;
		timerElapsed = 0;
		/* (PWM_LED_COUNT-1)*PWM_MAX_BRIGHTNESS states; -1, since two leds’s
		 * states are interleaved */
		brightnessStep = timerValue/(uint32_t) ((PWM_LED_COUNT-1)*PWM_MAX_BRIGHTNESS);
		fwrite (&brightnessStep, sizeof (brightnessStep), 1, stdout);

		currLed = PWM_LED_COUNT-1;
		brightness[currLed] = PWM_MAX_BRIGHTNESS;
		pwmSet (horizonLed (currLed), brightness[currLed]);
		timerStart (brightnessStep, false);
#endif
	}
}

/*	Main loop
 */
void uiLoop () {
#if 0
	/* LED test mode */
	pwmStart ();
	uint8_t i = 0;
	uint8_t brightness = 0;
	while (1) {
		pwmSetOff ();
		pwmSet (horizonLed (i), brightness);
		++i;
		if (i >= PWM_LED_COUNT) {
			i = 0;
			++brightness;
			if (brightness > PWM_MAX_BRIGHTNESS) {
				brightness = 0;
			}
		}
		_delay_ms (250);
	}
#endif

#if 0
	/* timer test mode */
	timerStart ((uint32_t) 10*1000, false);
	while (1) {
		uint32_t t;
		sleepwhile (timerHit () == 0);
		puts ("on");
		fwrite (&t, sizeof (t), 1, stdout);
		pwmSet (horizonLed (0), PWM_ON);

		sleepwhile (timerHit () == 0);
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

	while (1) {
		processSensors ();

		h = accelGetHorizon (&horizonChanged);

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

			case UIMODE_FLASH_ON:
				doFlashOn ();
				break;

			case UIMODE_FLASH_OFF:
				doFlashOff ();
				break;

			default:
				assert (0 && "invalid ui mode");
				break;
		}

		sleepwhile (wakeup == 0);

#if 0
		printf ("t=%i, h=%i, s=%i\n", gyroGetZTicks (), h,
				accelGetShakeCount ());
		const int32_t gyroval = gyroGetZAccum ();
		const int16_t gyroraw = gyroGetZRaw ();
		printf ("%li - %i\n", gyroval, gyroraw);
#endif
	}

	timerStop ();
	pwmStop ();
}

