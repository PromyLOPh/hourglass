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

