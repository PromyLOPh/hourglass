/*	speaker control, uses timer2
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdbool.h>

#include "speaker.h"

static bool value = false;

static void speakerSet () {
	if (value) {
		PORTD = PORTD | (1 << PD6);
	} else {
		PORTD = PORTD & ~(1 << PD6);
	}
}

ISR(TIMER2_OVF_vect) {
	value = !value;
	speakerSet ();
}

void speakerInit () {
	/* set PD6 to output */
	DDRD |= (1 << PD6);
	/* value is false */
	speakerSet ();
}

void speakerStart () {
	/* reset timer value */
	TCNT2 = 0;
	/* set normal mode timer0 */
	TCCR2A = 0;
	/* enable overflow interrupt */
	TIMSK2 = (1 << TOIE2);
	/* io clock with 1024 prescaler */
	TCCR2B = (TCCR2B & ~((1 << CS21)) | (1 << CS22) | (1 << CS20));
}

void speakerStop () {
	/* zero clock source */
	TCCR2B = 0;
	value = false;
	speakerSet ();
}

