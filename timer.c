#include "common.h"

#include <avr/io.h>
#include <avr/interrupt.h>

#include "timer.h"

volatile unsigned char hits, maxhits;

ISR(TIMER1_COMPA_vect) {
	++hits;
}

/*	Check if timer was hit, return time since last restart or 0 if not hit yet
 */
uint32_t timerHit () {
	if (hits >= maxhits) {
		const uint32_t ret = TCNT1 * (uint32_t) 128 +
				(uint32_t) hits * OCR1A * (uint32_t) 128;
		hits = 0;
		return ret;
	} else {
		return 0;
	}
}

/*	Start a timer that fires every t us
 */
void timerStart (const uint32_t t) {
	/* reset timer value */
	TCNT1 = 0;
	/* set ctc (compare timer and clear) mode (part 1) */
	TCCR1A = 0;
	/* enable compare match interrupt */
	TIMSK1 = (1 << OCIE1A);
	/* set compare value */
#if F_CPU == 8000000
	const uint32_t tdiv = t/128;
	if (tdiv > UINT16_MAX) {
#warning "fix this"
		assert (0);
	} else {
		/* with divider 1024 each clock tick is 128us with a rounding error of
		 * 64us per second/4ms per minute, since its a 16 bit timer we support
		 * up to 2**16-1 ticks, which is 8388480us (8s) */
		OCR1A = tdiv;
		maxhits = 1;
	}
#else
#error "cpu speed not supported"
#endif
	/* io clock with 1024 prescaler; ctc (part 2) */
	TCCR1B = (1 << CS12) | (1 << CS10) | (1 << WGM12);

	hits = 0;
}

void timerStop () {
	/* zero clock source */
	TCCR1B = 0;
}

