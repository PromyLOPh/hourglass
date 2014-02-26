#include <avr/io.h>
#include <avr/interrupt.h>

#include "timer.h"

volatile unsigned char count = 0;
volatile unsigned char hit = 0;

/* XXX: use high-res timer1 for doing this
 */
ISR(TIMER0_OVF_vect) {
	/* there seems to be no mode of operation which disconnects pin OC0A _and_
	 * clears the value */
	++count;
	switch (count) {
		case 1:
		case 2:
			TCNT0 = 0;
			break;

		/* three overflows happened, next one is a little shorter:
		 * F_CPU/prescaler-3*256=208.5625 -> 256-208=48 -> zero-based=47 */
		case 3:
			TCNT0 = 47;
			break;

		/* one second elapsed */
		case 4:
			TCNT0 = 0;
			count = 0;
			++hit;
			break;
	}
}

bool timerHit () {
	bool ret = hit != 0;
	hit = 0;
	return ret;
}

void timerStart () {
	count = 0;
	/* reset timer value */
	TCNT0 = 0;
	/* set normal mode timer0 */
	TCCR0A = 0;
	/* enable overflow interrupt */
	TIMSK0 = (1 << TOIE0);
	/* io clock with 1024 prescaler */
	TCCR0B = (TCCR0B & ~((1 << CS01)) | (1 << CS02) | (1 << CS00));
}

void timerStop () {
	/* zero clock source */
	TCCR0B = 0;
}

