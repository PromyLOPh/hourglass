/*	speaker control, uses timer2
 */

#include "common.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdbool.h>
#include <stdlib.h>

#include "speaker.h"

static volatile uint16_t count;

/*	Compare interrupt
 */
ISR(TIMER2_COMPA_vect) {
	PORTD ^= (1 << PD6);
	--count;
	if (count == 0) {
		speakerStop ();
	}
}

void speakerInit () {
	/* set PD6 to output */
	DDRD |= (1 << PD6);
	/* turn off */
	PORTD = PORTD & ~(1 << PD6);
}

void speakerStart (const speakerMode mode) {
	/* 12.8ms */
	count = 100;

	/* compare value (hit on every tick) */
	OCR2A = 1;
	/* reset timer value */
	TCNT2 = 0;
	/* set ctc mode */
	TCCR2A = (1 << WGM21);
	/* enable overflow interrupt */
	TIMSK2 = (1 << OCIE2A);
	/* io clock with 1024 prescaler -> ~4kHz tone, ctc part2 */
	TCCR2B = (1 << CS22) | (1 << CS21) | (1 << CS20);
}

void speakerStop () {
	/* zero clock source */
	TCCR2B &= ~(0x7);
	/* turn off */
	PORTD = PORTD & ~(1 << PD6);
}

