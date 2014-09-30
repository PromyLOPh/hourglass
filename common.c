#include "common.h"
#include "pwm.h"

volatile uint8_t wakeup = 0;

/*	shutdown device signaling internal error
 */
void shutdownError () {
	pwmSet (0, PWM_ON);
	for (uint8_t i = 1; i < PWM_LED_COUNT-1; i++) {
		pwmSet (i, PWM_OFF);
	}
	pwmSet (PWM_LED_COUNT-1, PWM_ON);
	sleep_enable ();
	while (1) {
		sleep_cpu ();
	}
}

