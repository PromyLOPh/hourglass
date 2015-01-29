#ifndef COMMON_H
#define COMMON_H

/* cpu runs at n mhz */
#define F_CPU 8000000

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
#define WAKE_ACCEL 0
#define WAKE_GYRO 1
#define WAKE_I2C 2
#define WAKE_TIMER 3

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

