#include <stdio.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

#include "i2c.h"
#include "gyro.h"

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

void gyroInit () {
	/* set PB1 to input, with pull-up */
	DDRB = (DDRB & ~((1 << PB1)));
	PORTB = (PORTB | (1 << PB1));
	/* enable interrupt PCI0 */
	PCICR = (1 << PCIE0);
	/* enable interrupts on PB1/PCINT1 */
	PCMSK0 = (1 << 1);
}

/* XXX: make nonblocking */
void gyroStart () {
	/* configuration:
	 * disable power-down-mode
	 * defaults
	 * enable drdy interrupt
	 * select 500dps
	 */
	uint8_t data[] = {0b00001111, 0b0, 0b00001000, 0b00010000};

	if (!twRequest (TWM_WRITE, L3GD20, L3GD20_CTRLREG1, data,
			sizeof (data)/sizeof (*data))) {
		printf ("cannot start write\n");
	}
	sleepwhile (twr.status == TWST_WAIT);
	printf ("final twi status was %i\n", twr.status);
}

bool gyroRead () {
	if (drdy) {
		drdy = false;

		if (!twRequest (TWM_READ, L3GD20, 0x28, (uint8_t *) val, 6)) {
			printf ("cannot start read\n");
		}

		return true;
	} else {
		return false;
	}
}

volatile const int16_t *gyroGet () {
	return val;
}

