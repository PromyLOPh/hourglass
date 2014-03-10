#ifndef COMMON_H
#define COMMON_H

/* cpu runs at 1mhz */
#define F_CPU 1000000

#define sleepwhile(cond) \
	sleep_enable (); \
	while (cond) { sleep_cpu (); } \
	sleep_disable ();

#endif /* COMMON_H */

