#ifndef COMMON_H
#define COMMON_H

/* cpu runs at n mhz */
#define F_CPU 8000000

#define sleepwhile(cond) \
	sleep_enable (); \
	while (cond) { sleep_cpu (); } \
	sleep_disable ();

#define __unused__ __attribute__ ((unused))
/* print assert messages */
#define __ASSERT_USE_STDERR

#endif /* COMMON_H */

