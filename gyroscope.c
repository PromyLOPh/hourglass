#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

#include "i2c.h"
#include "gyroscope.h"

/* device address */
#define L3GD20 0b11010100
/* registers */
#define L3GD20_WHOAMI 0xf
#define L3GD20_CTRLREG1 0x20
#define L3GD20_CTRLREG3 0x22
#define L3GD20_CTRLREG4 0x23

#define sleepwhile(cond) \
	sleep_enable (); \
	while (cond) { sleep_cpu (); } \
	sleep_disable ();

/* the first interrupt is lost */
volatile bool drdy = true;
volatile int16_t val[3] = {0, 0, 0};

/* data ready interrupt
 */
ISR(PCINT0_vect) {
	drdy = true;
}

void gyroscopeInit () {
	/* set PB1 to input, with pull-up */
	DDRB = (DDRB & ~((1 << PB1)));
	PORTB = (PORTB | (1 << PB1));
	/* enable interrupt PCI0 */
	PCICR = (1 << PCIE0);
	/* enable interrupts on PB1/PCINT1 */
	PCMSK0 = (1 << 1);
}

/* XXX: make nonblocking */
void gyroscopeStart () {
	/* XXX: implement multi-write */
	/* disable power-down-mode */
	if (!twWrite (L3GD20, L3GD20_CTRLREG1, 0b00001111)) {
		printf ("cannot start write\n");
	}
	sleepwhile (twr.status == TWST_WAIT);
	printf ("final twi status was %i\n", twr.status);

	/* enable drdy interrupt */
	if (!twWrite (L3GD20, L3GD20_CTRLREG3, 0b00001000)) {
		printf ("cannot start write\n");
	}
	sleepwhile (twr.status == TWST_WAIT);
	printf ("final twi status was %i\n", twr.status);

	/* select 500dps */
	if (!twWrite (L3GD20, L3GD20_CTRLREG4, 0b00010000)) {
		printf ("cannot start write\n");
	}
	sleepwhile (twr.status == TWST_WAIT);
	printf ("final twi status was %i\n", twr.status);
}

bool gyroscopeRead () {
	if (drdy) {
		drdy = false;

		if (!twReadMulti (L3GD20, 0x28, (uint8_t *) val, 6)) {
			printf ("cannot start read\n");
		}

		return true;
	} else {
		return false;
	}
}

volatile const int16_t *gyroscopeGet () {
	return val;
}

