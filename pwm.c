/*	LED pwm, uses timer0
 */

#include "common.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "pwm.h"

static uint8_t count = 0;
static uint8_t speakerCount = 0;
static uint8_t pwmvalue[PWM_MAX_BRIGHTNESS][2];
/* led bitfield, indicating which ones are pwm-controlled */
static const uint8_t offbits[2] = {(uint8_t) ~((1 << PB6) | (1 << PB7)),
		(uint8_t) ~((1 << PD2) | (1 << PD3) | (1 << PD4) | (1 << PD5) | (1 << PD6))};

ISR(TIMER0_COMPA_vect) {
#warning "speaker works now, led1 and 2 do not work"
	if (speakerCount > 0) {
		--speakerCount;
		/* stop speaker after beep */
		if (speakerCount == 0) {
			/* in speakerStart we set every 2nd to 1 */
			for (uint8_t i = 0; i < PWM_MAX_BRIGHTNESS; i += 2) {
				pwmvalue[i][1] &= ~(1 << PD6);
			}
		}
	}

	PORTB = pwmvalue[count][0];
	PORTD = pwmvalue[count][1];

	/* auto wrap-around */
	count = (count+1) & (PWM_MAX_BRIGHTNESS-1);
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
	memset (pwmvalue, 0, sizeof (pwmvalue));
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
	/* io clock with prescaler 64; ctc (part 2) */
	TCCR0B = (1 << CS02) | (0 << CS01) | (1 << CS00);
}

static void allLedsOff () {
	PORTB &= offbits[0];
	PORTD &= offbits[1];
}

void pwmStop () {
	/* zero clock source */
	TCCR0B = 0;
	allLedsOff ();
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

void speakerStart (const speakerMode mode) {
	/* 12.8ms */
	speakerCount = 100;
	for (uint8_t i = 0; i < PWM_MAX_BRIGHTNESS; i += 2) {
		pwmvalue[i][1] |= (1 << PD6);
	}
}

void speakerInit () {
	/* set PD6 to output */
	DDRD |= (1 << PD6);
	/* turn off */
	PORTD = PORTD & ~(1 << PD6);
}

