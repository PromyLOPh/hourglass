#include "common.h"

#include <avr/io.h>
#include <avr/interrupt.h>

#include "timer.h"

/* useconds per clock tick */
#define US_PER_TICK 128
/* max useconds with max OCRA1 */
#define MAX_US ((uint32_t) 8388480)

/* max/current timer hits */
volatile uint8_t maxhits, hits;
/* compare value for n-1, last hits */
uint16_t count, lastcount;
/* accumulated time since last timeout/call to timerHit (us) */
uint32_t time;

ISR(TIMER1_COMPA_vect) {
	++hits;
	time += OCR1A * US_PER_TICK;
	if (hits == maxhits-1) {
		OCR1A = lastcount;
	}
}

/*	Check if timer was hit, return time since last restart or 0 if not hit yet
 */
uint32_t timerHit () {
	if (hits >= maxhits) {
		const uint32_t ret = time;
		/* reset timer, start again */
		hits = 0;
		time = 0;
		OCR1A = count;
		TCNT1 = 0;
		return ret;
	} else {
		return 0;
	}
}

/*	Start a timer that fires every t us
 */
void timerStart (const uint32_t t) {
	hits = 0;
	time = 0;

	/* reset timer value */
	TCNT1 = 0;
	/* set ctc (compare timer and clear) mode (part 1) */
	TCCR1A = 0;
	/* enable compare match interrupt */
	TIMSK1 = (1 << OCIE1A);
	/* set compare value */
#if F_CPU == 8000000
	/* with divider 1024 each clock tick is 128us with a rounding error of 64us
	 * per second/4ms per minute, since it’s a 16 bit timer we support up to
	 * 2**16-1 ticks, which is 8388480us (8s) */
	if (t >= MAX_US) {
		maxhits = t/MAX_US+1;
		count = UINT16_MAX;
		lastcount = (t % MAX_US) / US_PER_TICK;
		OCR1A = UINT16_MAX;
	} else {
		maxhits = 1;
		const uint16_t tdiv = t / US_PER_TICK;
		count = tdiv;
		lastcount = tdiv;
		OCR1A = tdiv;
	}
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

