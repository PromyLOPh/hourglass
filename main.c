/* cpu runs at 1mhz */
#define F_CPU 1000000

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <util/twi.h>
#include <stdio.h>
#include <stdbool.h>

/* i2c device addresses */
#define L3GD20 0b11010100
#define L3GD20_WHOAMI 0xf
#define L3GD20_CTRLREG1 0x20
#define LIS302DL 0b00111000
#define LIS302DL_WHOAMI 0xf
#define LIS302DL_CTRLREG1 0x20

typedef enum {
	TWM_INVALID = 0,
	TWM_WRITE,
	TWM_READ_MULTI,
} twMode;

typedef enum {
	TWST_WAIT = 0,
	TWST_OK = 1,
	TWST_ERR = 2,
} twStatus;

typedef struct {
	twMode mode;
	uint8_t address;
	uint8_t subaddress;
	uint8_t data;
	uint8_t step;
	/* read data store */
	uint8_t *retData;
	/* number of bytes to be read */
	uint8_t count;
	/* current byte */
	uint8_t i;
	twStatus status;
} twReq;

volatile twReq twr;

void twStartRaw () {
	/* disable stop, enable interrupt, reset twint, enable start, enable i2c */
	TWCR = (TWCR & ~(1 << TWSTO)) | (1 << TWIE) | (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
}

void twStopRaw () {
	/* disable start, enable interrupt, reset twint, enable stop, enable i2c */
	TWCR = (TWCR & ~(1 << TWSTA)) | (1 << TWIE) | (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
}

void twFlushRaw () {
	/* disable start/stop, enable interrupt, reset twint, enable i2c */
	TWCR = (TWCR & ~((1 << TWSTA) | (1 << TWSTO) | (1 << TWEA))) | (1 << TWIE) | (1 << TWINT) | (1 << TWEN);
}

/* flush and send master ack */
void twFlushContRaw () {
	/* disable start/stop, enable interrupt, reset twint, enable i2c, send master ack */
	TWCR = (TWCR & ~((1 << TWSTA) | (1 << TWSTO))) | (1 << TWIE) | (1 << TWINT) | (1 << TWEN) | (1 << TWEA);
}

void twWaitRaw () {
	while (!(TWCR & (1 << TWINT)));
}

bool twWriteRaw (uint8_t data) {
	TWDR = data;
	if (TWCR & (1 << TWWC)) {
		printf("write collision\n");
		return false;
	} else {
		return true;
	}
}

void twInit () {
	/* set scl to 3.6 kHz (at 1Mhz CPU speed)*/
	TWBR = 2;
	TWSR |= 0x3; /* set prescaler to 64 */

	twr.mode = TWM_INVALID;
	twr.status = TWST_ERR;
}

/*	high-level write
 */
static bool twWrite (uint8_t address, uint8_t subaddress, uint8_t data) {
	/* do not start if request is pending */
	if (twr.status == TWST_WAIT) {
		return false;
	}

	twr.mode = TWM_WRITE;
	twr.address = address;
	twr.subaddress = subaddress;
	twr.data = data;
	twr.step = 0;
	twr.status = TWST_WAIT;
	/* wait for stop finish */
	while (TW_STATUS != 0xf8);
	twStartRaw ();

	return true;
}

static bool twReadMulti (uint8_t address, uint8_t subaddress,
		uint8_t *retData, uint8_t count) {
	/* do not start if request is pending */
	if (twr.status == TWST_WAIT) {
		return false;
	}

	twr.mode = TWM_READ_MULTI;
	twr.address = address;
	twr.subaddress = subaddress;
	twr.retData = retData;
	twr.count = count;
	twr.i = 0;
	twr.step = 0;
	twr.status = TWST_WAIT;
	/* wait for stop finish */
	while (TW_STATUS != 0xf8);
	twStartRaw ();

	return true;
}

void ledInit () {
	/* set led1,led2 to output */
	DDRB |= (1 << PB6) | (1 << PB7);
	/* set led3,led4,led5,led6 to output */
	DDRD |= (1 << PD2) | (1 << PD3) | (1 << PD4) | (1 << PD5);
}

/* show data with leds */
void ledShow (unsigned char val) {
	PORTB = (PORTB & ~((1 << PB6) | (1 << PB7))) | ((val & 0x3) << PB6);
	PORTD = (PORTD & ~((1 << PD2) | (1 << PD3) | (1 << PD4) | (1 << PD5))) | (((val >> 2) & 0xf) << PD2);
}

void uartInit () {
	/* Set baud rate (9600, double speed, at 1mhz) */
	UBRR0H = 0;
	UBRR0L = 12;
	/* enable double speed mode */
	UCSR0A = (1 << U2X0);
	/* Enable receiver and transmitter */
	UCSR0B = (1<<RXEN0)|(1<<TXEN0);
	/* Set frame format: 8 data, 1 stop bit, even parity */
	UCSR0C = (1<<UPM01) | (0 << UPM00) | (0<<USBS0)|(3<<UCSZ00);
}

/* blocking uart send
 */
void uartSend (unsigned char data) {
	/* Wait for empty transmit buffer */
	while (!( UCSR0A & (1<<UDRE0)));
	/* Put data into buffer, sends the data */
	UDR0 = data;
}

int uartPutc (char c, FILE *stream) {
	if (c == '\n') {
		uartSend ('\r');	
	}
	uartSend (c);
	return 0;
}

static FILE mystdout = FDEV_SETUP_STREAM (uartPutc, NULL, _FDEV_SETUP_WRITE);

unsigned char uartReceive () {
	/* Wait for data to be received */
	while ( !(UCSR0A & (1<<RXC0)) );
	/* Get and return received data from buffer */
	return UDR0;
}

void cpuInit () {
	/* enter change prescaler mode */
	CLKPR = CLKPCE << 1;
	/* write new prescaler = 8 (i.e. 1Mhz clock frequency) */
	CLKPR = 0b00000011;
}

/*	handle interrupt, write request
 */
static void twIntWrite () {
	switch (twr.step) {
		case 0:
			if (TW_STATUS == TW_START) {
				twWriteRaw (twr.address | TW_WRITE);
				twFlushRaw ();
			} else {
				twr.status = TWST_ERR;
			}
			break;

		case 1:
			if (TW_STATUS == TW_MT_SLA_ACK) {
				twWriteRaw (twr.subaddress);
				twFlushRaw ();
			} else {
				twr.status = TWST_ERR;
			}
			break;

		case 2:
			if (TW_STATUS == TW_MT_DATA_ACK) {
				twWriteRaw (twr.data);
				twFlushRaw ();
			} else {
				twr.status = TWST_ERR;
			}
			break;

		case 3:
			if (TW_STATUS == TW_MT_DATA_ACK) {
				twStopRaw ();
				twr.status = TWST_OK;
			} else {
				twr.status = TWST_ERR;
			}
			break;

		default:
			printf ("nope\n");
			break;
	}
	++twr.step;
}

static void twIntReadMulti () {
	switch (twr.step) {
		case 0:
			if (TW_STATUS == TW_START) {
				/* write device address */
				twWriteRaw (twr.address | TW_WRITE);
				twFlushRaw ();
				++twr.step;
			} else {
				twr.status = TWST_ERR;
			}
			break;

		case 1:
			if (TW_STATUS == TW_MT_SLA_ACK) {
				/* write subaddress, enable auto-increment */
				twWriteRaw ((1 << 7) | twr.subaddress);
				twFlushRaw ();
				++twr.step;
			} else {
				twr.status = TWST_ERR;
			}
			break;

		case 2:
			if (TW_STATUS == TW_MT_DATA_ACK) {
				/* send repeated start */
				twStartRaw ();
				++twr.step;
			} else {
				twr.status = TWST_ERR;
			}
			break;

		case 3:
			if (TW_STATUS == TW_REP_START) {
				/* now start the actual read request */
				twWriteRaw (twr.address | TW_READ);
				twFlushRaw ();
				++twr.step;
			} else {
				twr.status = TWST_ERR;
			}
			break;

		case 4:
			if (TW_STATUS == TW_MR_SLA_ACK) {
				/* send master ack if next data block is received */
				twFlushContRaw ();
				++twr.step;
			} else {
				twr.status = TWST_ERR;
			}

		case 5:
			if (TW_STATUS == TW_MR_DATA_ACK) {
				twr.retData[twr.i] = TWDR;
				++twr.i;
				if (twr.i < twr.count-1) {
					/* read another byte, not the last one */
					twFlushContRaw ();
					/* step stays the same */
				} else {
					/* read last byte, send master nack */
					twFlushRaw ();
					++twr.step;
				}
			} else {
				twr.status = TWST_ERR;
			}
			break;

		case 6:
			if (TW_STATUS == TW_MR_DATA_NACK) {
				/* receive final byte, send stop */
				twr.retData[twr.i] = TWDR;
				twStopRaw ();
				twr.status = TWST_OK;
			} else {
				twr.status = TWST_ERR;
			}
			break;

		default:
			printf ("twIntReadMulti: nope\n");
			break;
	}
}

ISR(TWI_vect) {
	switch (twr.mode) {
		case TWM_WRITE:
			twIntWrite ();
			break;

		case TWM_READ_MULTI:
			twIntReadMulti ();
			break;

		default:
			printf ("nope\n");
			break;
	}
}

int main(void) {
	cpuInit ();
	ledInit ();
	twInit ();
	uartInit ();
	
	/* redirect stdout */
	stdout = &mystdout;
	
	printf ("initialization done\n");

	/* global interrupt enable */
	sei ();
	/* disable power-down-mode */
	if (!twWrite (LIS302DL, LIS302DL_CTRLREG1, 0b01000111)) {
		printf ("cannot start write\n");
	}
	while (twr.status == TWST_WAIT);
	printf ("final twi status was %i\n", twr.status);

	while (1) {
		uint8_t val[6];
		if (!twReadMulti (LIS302DL, 0x28, val, 6)) {
			printf ("cannot start read\n");
		}
		while (twr.status == TWST_WAIT);
		printf ("%i/%i/%i\n", (int8_t) val[1], (int8_t) val[3], (int8_t) val[5]);
		/* XXX: why do we need the delay here? */
		_delay_ms (250);
	}
	/* global interrupt disable */
	cli ();

	while (1);
}

