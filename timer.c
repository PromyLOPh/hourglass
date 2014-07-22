#include "common.h"

#include <avr/io.h>
#include <avr/interrupt.h>

#include "timer.h"

/* counts one second, off by +1s per 2m
 */

volatile uint16_t count = 0;
volatile unsigned char hit = 0;

ISR(TIMER1_COMPA_vect) {
	++hit;
}

bool timerHit () {
	bool ret = hit != 0;
	hit = 0;
	return ret;
}

void timerStart () {
	count = 0;
	/* reset timer value */
	TCNT1 = 0;
	/* set ctc (compare timer and clear) mode (part 1) */
	TCCR1A = 0;
	/* enable compare match interrupt */
	TIMSK1 = (1 << OCIE1A);
	/* set compare value */
#if F_CPU == 8000000
	OCR1A = 7812;
#else
#error "cpu speed not supported"
#endif
	/* io clock with 1024 prescaler; ctc (part 2) */
	TCCR1B = (1 << CS12) | (1 << CS10) | (1 << WGM12);
}

void timerStop () {
	/* zero clock source */
	TCCR1B = 0;
}

