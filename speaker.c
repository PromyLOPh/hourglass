/*	speaker control, uses timer2
 */

#include "common.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdbool.h>
#include <assert.h>
#include <stdlib.h>

#include "speaker.h"

ISR(TIMER2_OVF_vect) {
	PORTD ^= (1 << PD6);
}

void speakerInit () {
	/* set PD6 to output */
	DDRD |= (1 << PD6);
	/* turn off */
	PORTD = PORTD & ~(1 << PD6);
}

void speakerStart (const speakerMode mode) {
	/* reset timer value */
	TCNT2 = 0;
	/* set normal mode */
	TCCR2A = 0;
	/* enable overflow interrupt */
	TIMSK2 = (1 << TOIE2);
	/* io clock with 1024 prescaler */
	TCCR2B = ((TCCR2B & ~(1 << CS21)) | (1 << CS22) | (1 << CS20));
	_delay_ms (50);
	speakerStop ();
}

void speakerStop () {
	/* zero clock source */
	TCCR2B = 0;
	/* turn off */
	PORTD = PORTD & ~(1 << PD6);
}

