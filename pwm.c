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

/*	LED pwm, uses timer0
 */

#include "common.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "pwm.h"

/* Slow mode for LEDs only */
#define SLOW_PRESCALER ((1 << CS02) | (0 << CS01) | (1 << CS00))
/* Fast mode with speaker */
#define FAST_PRESCALER ((1 << CS02) | (0 << CS01) | (0 << CS00))

static uint8_t count = 0;
static uint8_t speakerCount = 0;
static uint8_t pwmvalue[PWM_MAX_BRIGHTNESS][2];
/* inversed(!) bitfield, indicating which LEDs are pwm-controlled */
static const uint8_t notledbits[2] = {(uint8_t) ~((1 << PB6) | (1 << PB7)),
		(uint8_t) ~((1 << PD2) | (1 << PD3) | (1 << PD4) | (1 << PD5))};

static void enableSlow () {
	TCCR0B = SLOW_PRESCALER;
}

static void enableFast () {
	TCCR0B = FAST_PRESCALER;
}

ISR(TIMER0_COMPA_vect) {
	if (speakerCount > 0) {
		--speakerCount;
		/* stop speaker after beep */
		if (speakerCount == 0) {
			/* in speakerStart we set every 2nd to 1 */
			for (uint8_t i = 0; i < PWM_MAX_BRIGHTNESS; i += 2) {
				pwmvalue[i][1] &= ~(1 << PD6);
			}
			enableSlow ();
		}
	}

	PORTB = pwmvalue[count][0];
	PORTD = pwmvalue[count][1];

	/* auto wrap-around */
	count = (count+1) & (PWM_MAX_BRIGHTNESS-1);

	/* no wakeup */
}

static uint8_t ledToArray (const uint8_t i) {
	assert (i < PWM_LED_COUNT);
	if (i >= 2) {
		return 1;
	} else {
		return 0;
	}
}

static uint8_t ledToShift (const uint8_t i) {
	assert (i < PWM_LED_COUNT);
	static const uint8_t shifts[] = {PB6, PB7, PD2, PD3, PD4, PD5};
	return shifts[i];
}

void pwmInit () {
	/* set led1,led2 to output */
	DDRB |= (1 << PB6) | (1 << PB7);
	/* set led3,led4,led5,led6 to output */
	DDRD |= (1 << PD2) | (1 << PD3) | (1 << PD4) | (1 << PD5);

	/* gyro uses pb1, get its setup; this function must be called after gyro setup */
	const uint8_t pbdef = PORTB;
	const uint8_t pddef = PORTD;
	for (uint8_t i = 0; i < PWM_MAX_BRIGHTNESS; i++) {
		pwmvalue[i][0] = pbdef;
		pwmvalue[i][1] = pddef;
	}
}

void pwmStart () {
	count = 0;
	/* reset timer value */
	TCNT0 = 0;
	/* set ctc timer0 (part 1) */
	TCCR0A = (1 << WGM01);
	/* enable compare match interrupt */
	TIMSK0 = (1 << OCIE0A);
	/* compare value */
	OCR0A = 1;
	enableSlow ();
}

void pwmStop () {
	/* zero clock source */
	TCCR0B = 0;
	PORTB &= notledbits[0];
	PORTD &= notledbits[1];
}

/*	Set LED brightness
 *
 *	We could switch off interrupts here. Instead use pwmStart/Stop.
 */
void pwmSet (const uint8_t i, const uint8_t value) {
	assert (i < PWM_LED_COUNT);
	assert (value <= PWM_MAX_BRIGHTNESS);

	const uint8_t array = ledToArray (i);
	const uint8_t bit = 1 << ledToShift (i);

	for (uint8_t j = 0; j < value; j++) {
		pwmvalue[j][array] |= bit;
	}
	for (uint8_t j = value; j < PWM_MAX_BRIGHTNESS; j++) {
		pwmvalue[j][array] &= ~bit;
	}
}

/*	Switch all LEDs off
 */
void pwmSetOff () {
	for (uint8_t i = 0; i < PWM_MAX_BRIGHTNESS; i++) {
		pwmvalue[i][0] &= notledbits[0];
		pwmvalue[i][1] &= notledbits[1];
	}
}

void speakerStart (const speakerMode mode __unused__) {
	enableFast ();
	for (uint8_t i = 0; i < PWM_MAX_BRIGHTNESS; i += 2) {
		pwmvalue[i][1] |= (1 << PD6);
	}
	/* 12.8ms */
	speakerCount = 100;
}

void speakerInit () {
	/* set PD6 to output */
	DDRD |= (1 << PD6);
	/* turn off */
	PORTD = PORTD & ~(1 << PD6);
}

