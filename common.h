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
		sleep_enable (); \
		while (1) { \
			sleep_cpu (); \
		} \
	}

#endif /* COMMON_H */

