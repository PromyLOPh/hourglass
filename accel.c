#include <stdio.h>
#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

#include "i2c.h"
#include "accel.h"

/* device address */
#define LIS302DL 0b00111000
/* registers */
#define LIS302DL_WHOAMI 0xf
#define LIS302DL_CTRLREG1 0x20
#define LIS302DL_UNUSED1 0x28

#define sleepwhile(cond) \
	sleep_enable (); \
	while (cond) { sleep_cpu (); } \
	sleep_disable ();

/* the first interrupt is lost */
static volatile bool drdy = true;
/* 0, 2 and 4 are zero, as they contain the dummy register’s content */
static volatile int8_t val[6] = {0, 0, 0, 0, 0, 0};
/* currently reading from i2c */
static bool reading = false;

#warning "wrong interrupt"
/* data ready interrupt
 */
ISR(PCINT1_vect) {
	drdy = true;
}

void accelInit () {
	/* set PB1 to input, with pull-up */
	//DDRB = (DDRB & ~((1 << PB1)));
	//PORTB = (PORTB | (1 << PB1));
	/* enable interrupt PCI0 */
	//PCICR = (1 << PCIE0);
	/* enable interrupts on PB1/PCINT1 */
	//PCMSK0 = (1 << 1);
}

/* XXX: make nonblocking */
void accelStart () {
	/* configuration:
	 * disable power-down-mode
	 * defaults
	 * data ready interrupt on int2
	 */
	uint8_t data[] = {0b01000111, 0b0, 0b00100000};

	if (!twRequest (TWM_WRITE, LIS302DL, LIS302DL_CTRLREG1, data,
			sizeof (data)/sizeof (*data))) {
		printf ("cannot start write\n");
	}
	sleepwhile (twr.status == TWST_WAIT);
	printf ("final twi status was %i\n", twr.status);
}

bool accelProcess () {
	if (reading) {
		if (twr.status == TWST_OK) {
			/* new data transfered */
			reading = false;
			return true;
		} else if (twr.status == TWST_ERR) {
			printf ("accel i2c error\n");
			reading = false;
		}
	} else {
		if (drdy && twr.status != TWST_WAIT) {
			/* new data available in device buffer and bus is free, we are
			 * reading the registers inbetween out_x/y/z and ignore them */
			if (!twRequest (TWM_READ, LIS302DL, LIS302DL_UNUSED1, (uint8_t *) val, 6)) {
				printf ("cannot start read\n");
			} else {
				drdy = false;
				reading = true;
			}
		}
	}

	return false;
}

