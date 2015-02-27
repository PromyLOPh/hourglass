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

#ifndef COMMON_H
#define COMMON_H

/* cpu runs at n mhz */
#define F_CPU 1000000

#define sleepwhile(cond) \
	sleep_enable (); \
	while (cond) { sleep_cpu (); } \
	sleep_disable ();

#define __unused__ __attribute__ ((unused))

/* define lightweight assert (without printf) that halts the cpu */
#include <stdio.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#define assert(x) if (!(x)) { \
		puts("assert in " __FILE__ ":" #x); \
		shutdownError (); \
	}

#include <stdbool.h>

/* global wakeup flag, incremented by functions that interact with the main
 * loop (i.e. not pwm) */
extern volatile uint8_t wakeup;

/* wakeup sources */
#define WAKE_ACCEL_HORIZON 0
#define WAKE_ACCEL_SHAKE 1
#define WAKE_GYRO 2
#define WAKE_I2C 3
#define WAKE_TIMER 4

#define shouldWakeup(x) (wakeup & (1 << x))
#define enableWakeup(x) wakeup |= 1 << x;
#include <util/atomic.h>
#define disableWakeup(x) \
	ATOMIC_BLOCK (ATOMIC_FORCEON) { \
		wakeup &= ~(1 << x); \
	}

void shutdownError ();

#define sign(x) ((x < 0) ? -1 : 1)

#endif /* COMMON_H */

