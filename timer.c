/*
Copyright (c) 2014-2015
	Lars-Dominik Braun <lars@6xq.net>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include "common.h"

#include <avr/io.h>
#include <avr/interrupt.h>

#include "timer.h"

/* prescaler is 1024 */
#define PRESCALER ((1 << CS12) | (1 << CS10))

#if F_CPU == 1000000
/* with divider 1024 each tick is 1024us */
#define US_PER_TICK 1024
#define MAX_US ((uint32_t) 67107840)
#elif F_CPU == 8000000
/* with divider 1024 each clock tick is 128us with a rounding error of 64us
 * per second/4ms per minute, since it’s a 16 bit timer we support up to
 * 2**16-1 ticks, which is 8388480us (8s) */
#define US_PER_TICK 128
#define MAX_US ((uint32_t) 8388480)
#else
#error "cpu speed not supported"
#endif

/* max/current timer hits */
static volatile uint8_t maxhits, hits;
/* compare value for n-1, last hits */
static uint16_t count, lastcount;
/* accumulated time since last timeout/call to timerHit (us) */
static uint32_t time;
static bool oneshot = false;

ISR(TIMER1_COMPA_vect) {
	++hits;
	time += (uint32_t) OCR1A * (uint32_t) US_PER_TICK;
	if (hits == maxhits-1) {
		OCR1A = lastcount;
	} else if (hits >= maxhits) {
		enableWakeup (WAKE_TIMER);
		if (oneshot) {
			timerStop ();
		}
	}
}

/*	Check if timer was hit, return time since last restart or 0 if not hit yet
 */
uint32_t timerHit () {
	uint32_t ret = 0;
	ATOMIC_BLOCK (ATOMIC_FORCEON) {
		if (shouldWakeup (WAKE_TIMER)) {
			disableWakeup (WAKE_TIMER);

			ret = time;
			if (!oneshot) {
				/* reset timer, start again */
				hits = 0;
				time = 0;
				OCR1A = count;
				TCNT1 = 0;
			}
		}
	}
	return ret;
}

/*	Start a timer that fires every t us
 */
void timerStart (const uint32_t t, const bool once) {
	oneshot = once;
	hits = 0;
	time = 0;

	/* reset timer value */
	TCNT1 = 0;
	/* set ctc (compare timer and clear) mode (part 1) */
	TCCR1A = 0;
	/* enable compare match interrupt */
	TIMSK1 = (1 << OCIE1A);
	/* set compare value */
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

	/* prescaler and ctc (part 2) */
	TCCR1B = PRESCALER | (1 << WGM12);
}

void timerStop () {
	/* zero clock source */
	TCCR1B = 0;
}

