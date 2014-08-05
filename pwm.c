/*	LED pwm, uses timer0
 */

#include "common.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "speaker.h"
#include "pwm.h"

static uint8_t count = 0;
static uint8_t toggle[PWM_MAX_BRIGHTNESS][2];
/* led bitfield, indicating which ones are leds */
static const uint8_t bits[2] = {(1 << PB6) | (1 << PB7),
		(1 << PD2) | (1 << PD3) | (1 << PD4) | (1 << PD5)};

static void allLedsOff () {
	PORTB &= ~bits[0];
	PORTD &= ~bits[1];
}

ISR(TIMER0_COMPA_vect) {
	/* the 16th+ state is always ignored/force-off */
	if (count >= PWM_MAX_BRIGHTNESS-1) {
		allLedsOff ();
	} else {
		PORTB ^= toggle[count][0];
		PORTD ^= toggle[count][1];
	}
	/* 16 steps */
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

#if 0
static void ledOff (const uint8_t i) {
	assert (i < PWM_LED_COUNT);
	if (ledToArray (i) == 0) {
		PORTB = PORTB & ~(1 << ledToShift (i));
	} else {
		PORTD = PORTD & ~(1 << ledToShift (i));
	}
}
#endif

void pwmInit () {
	/* set led1,led2 to output */
	DDRB |= (1 << PB6) | (1 << PB7);
	/* set led3,led4,led5,led6 to output */
	DDRD |= (1 << PD2) | (1 << PD3) | (1 << PD4) | (1 << PD5);
	memset (toggle, 0, sizeof (toggle));
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
	OCR0A = 255;
	/* io clock with prescaler 64; ctc (part 2) */
	TCCR0B = (0 << CS02) | (1 << CS01) | (1 << CS00);
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
	const uint8_t array = ledToArray (i);
	const uint8_t bit = 1 << ledToShift (i);
	/* disable all toggles */
	for (uint8_t j = 0; j < PWM_MAX_BRIGHTNESS; j++) {
		toggle[j][array] &= ~bit;
	}
	uint8_t toggleat;
	if (value < PWM_MAX_BRIGHTNESS) {
		toggleat = PWM_MAX_BRIGHTNESS-value;
	} else {
		/* max brightness */
		toggleat = 0;
	}
	toggle[toggleat][array] |= bit;
}

