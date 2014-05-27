/*	LED pwm, uses timer0
 *	XXX: this works, but we should use non-linear steps
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdbool.h>

#include "speaker.h"

#define PWM_STEPS 16

/* inverse brightness; 0 is max brightness, PWM_STEPS off */
static uint8_t invbrightness[6];
static uint8_t count = 0;

ISR(TIMER0_COMPA_vect) {
	if (count == 0) {
		/* switch off all LEDs */
		PORTB = PORTB & ~((1 << PB6) | (1 << PB7));
		PORTD = PORTD & ~((1 << PD2) | (1 << PD3) | (1 << PD4) | (1 << PD5));
	} else {
		uint8_t pb = 0, pd = 0;
		if (count > invbrightness[0]) {
			pb |= (1 << PB6);
		}
		if (count > invbrightness[1]) {
			pb |= (1 << PB7);
		}
		if (count > invbrightness[2]) {
			pd |= (1 << PD2);
		}
		if (count > invbrightness[3]) {
			pd |= (1 << PD3);
		}
		if (count > invbrightness[4]) {
			pd |= (1 << PD4);
		}
		if (count > invbrightness[5]) {
			pd |= (1 << PD5);
		}
		/* actually set them */
		PORTB |= pb;
		PORTD |= pd;
	}
	++count;
	if (count >= PWM_STEPS) {
		count = 0;
	}
}

void pwmInit () {
	/* set led1,led2 to output */
	DDRB |= (1 << PB6) | (1 << PB7);
	/* set led3,led4,led5,led6 to output */
	DDRD |= (1 << PD2) | (1 << PD3) | (1 << PD4) | (1 << PD5);
}

void pwmStart () {
	/* reset timer value */
	TCNT0 = 0;
	/* set ctc timer0 (part 1) */
	TCCR0A = 0;
	/* enable compare match interrupt */
	TIMSK0 = (1 << OCIE0A);
	/* compare value; interrupt on every tick */
	OCR0A = 0;
	/* io clock with 8 prescaler (>8 is too slow and starts flickering); ctc
	 * (part 2) */
	TCCR0B = (1 << CS01) | (1 << WGM02);
}

void pwmStop () {
	/* zero clock source */
	TCCR0B = 0;
}

void pwmSetBrightness (const uint8_t i, const uint8_t b) {
	invbrightness[i] = b;
}

