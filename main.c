#include "common.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include "i2c.h"
#include "uart.h"
#include "timer.h"
#include "gyro.h"
#include "accel.h"
#include "pwm.h"
#include "ui.h"

static void cpuInit () {
	/* enter change prescaler mode */
	CLKPR = (1 << CLKPCE);
	/* write new prescaler */
#if F_CPU == 1000000
	CLKPR = 0b00000011;
#elif F_CPU == 4000000
	CLKPR = 0b00000001;
#elif F_CPU == 8000000
	CLKPR = 0b00000000;
#else
#error "cpu speed not supported"
#endif
}

int main () {
	cpuInit ();
	twInit ();
	uartInit ();
	gyroInit ();
	accelInit ();
	/* pwm must be last, see pwm.c */
	pwmInit ();
	set_sleep_mode (SLEEP_MODE_IDLE);

	sei ();
	uiLoop ();
	cli ();

	while (1);
}

