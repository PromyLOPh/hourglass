/*	LED pwm, uses timer0
 */

#include "common.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdbool.h>
#include <assert.h>
#include <stdlib.h>

#include "speaker.h"
#include "pwm.h"

/* max count for blinks */
static uint8_t blink[6];
static uint8_t comphit = 0;
static uint8_t state = 0;
/* PORTB and PORTB values */
static uint8_t val[2];
static uint8_t init[2];

static void ledOff () {
	PORTB = PORTB & ~((1 << PB6) | (1 << PB7));
	PORTD = PORTD & ~((1 << PD2) | (1 << PD3) | (1 << PD4) | (1 << PD5));
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

/*	All LEDs are off for state % 2 == 0 (off state) or state >= 7 (end of blink
 *	sequence), setting blink[i] = state*2 causes LED i to blink state times
 */
ISR(TIMER0_COMPA_vect) {
	++comphit;
	/* divide by 13 to get ~10 Hz timer */
	if (comphit >= 13) {
		comphit = 0;
		++state;
		if (state == 12) {
			state = 0;
		}
		val[0] = init[0];
		val[1] = init[1];
		if (state >= 10 || state % 2 == 0) {
			/* end of blink/off state */
		} else {
			for (uint8_t i = 0; i < PWM_LED_COUNT; i++) {
				if (state < blink[i]) {
					val[ledToArray (i)] |= (1 << ledToShift(i));
				}
			}
		}
	}

	if (comphit % 2 == 0) {
		ledOff ();
	} else {
		PORTB |= val[0];
		PORTD |= val[1];
	}
}

void pwmInit () {
	/* set led1,led2 to output */
	DDRB |= (1 << PB6) | (1 << PB7);
	/* set led3,led4,led5,led6 to output */
	DDRD |= (1 << PD2) | (1 << PD3) | (1 << PD4) | (1 << PD5);
}

void pwmStart () {
	comphit = 0;
	state = 0;
	/* reset timer value */
	TCNT0 = 0;
	/* set ctc timer0 (part 1) */
	TCCR0A = (1 << WGM01);
	/* enable compare match interrupt */
	TIMSK0 = (1 << OCIE0A);
	/* compare value */
	OCR0A = 255;
	/* io clock with prescaler 256; ctc (part 2) */
	TCCR0B = (1 << CS02) | (0 << CS01) | (0 << CS00);
}

void pwmStop () {
	/* zero clock source */
	TCCR0B = 0;
	ledOff ();
}

void pwmSetBlink (const uint8_t i, const uint8_t value) {
	assert (i < PWM_LED_COUNT);
	if (value == PWM_BLINK_ON) {
		/* permanently switch on LED */
		init[ledToArray (i)] |= (1 << ledToShift (i));
	} else {
		init[ledToArray (i)] &= ~(1 << ledToShift (i));
		blink[i] = value*2;
	}
}

