/* cpu freq */
#define F_CPU 1000000

#include <avr/io.h>
#include <util/delay.h>
#include <util/twi.h>
#include <stdio.h>

/* cpu runs at 1mhz */

/* i2c device addresses */
#define L3GD20 0b11010100
#define L3GD20_WHOAMI 0xf
#define L3GD20_CTRLREG1 0x20
#define LIS302DL 0b00111000
#define LIS302DL_WHOAMI 0xf
#define LIS302DL_CTRLREG1 0x20

void twStart () {
	/* disable stop, reset twint, enable start, enable i2c */
	TWCR = (TWCR & ~(1 << TWSTO)) | (1 << TWINT) | (1 << TWSTA) | (1 << TWEN);
}

void twStop () {
	/* disable start, reset twint, enable stop, enable i2c */
	TWCR = (TWCR & ~(1 << TWSTA)) | (1 << TWINT) | (1 << TWSTO) | (1 << TWEN);
}

void twFlush () {
	/* disable start/stop, reset twint, enable i2c */
	TWCR = (TWCR & ~((1 << TWSTA) | (1 << TWSTO))) | (1 << TWINT) | (1 << TWEN);
}

void twWait () {
	while (!(TWCR & (1 << TWINT)));
}

void twInit () {
	/* set scl to 3.6 kHz (at 1Mhz CPU speed)*/
	TWBR = 2;
	TWSR |= 0x3; /* set prescaler to 64 */
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

#if 0
void test1 () {
	while (1) {
		unsigned char res = uartReceive ();
		for (unsigned char i = 0; i < 3; i++) {
			PORTB ^= (1 << PB6);
			_delay_ms (1000);
		}
		/* no frame or parity error? */
		if (!(UCSR0A & (1 << FE0)) && !(UCSR0A & (1 << UPE0))) {
			ledShow (res);
		}
		uartSend (res);
		/* check tx complete */
		while ( !(UCSR0A & (1<<TXC0)) );
		/* clear tx complete */
		UCSR0A |= (1<<TXC0);
	}
}
#endif

#if 0
void enumreg () {
	printf ("enumerating registers\n");
	unsigned char reg[] = {0xf, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37};
	//unsigned char reg[] = {0xf, 0x20, 0x21, 0x22, 0x23, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2d, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f};
	for (unsigned char i = 0; i < sizeof (reg)/sizeof(*reg); i++) {
		printf ("start\n");
		twStart ();
		twWait ();
		unsigned char status = 0x0;
		
		/* check status code */
		if ((status = TW_STATUS) == 0x8) {
			printf ("start ok\nwriting dev address\n");
			
			/* write device address and write bit */
			TWDR = L3GD20 | TW_WRITE;
			if (TWCR & (1 << TWWC)) {
				printf("write collision\n");
			}
			twFlush ();
			twWait ();
			if ((status = TW_STATUS) == 0x18) {
				printf ("ok\n");
				} else {
				printf ("fail with code %x\n", status);
			}

			printf ("writing subaddress %x\n", reg[i]);
			
			/* write subaddress (actually i2c data) */
			TWDR = reg[i];
			if (TWCR & (1 << TWWC)) {
				printf("write collision\n");
			}
			twFlush ();
			twWait ();
			if ((status = TW_STATUS) == 0x28) {
				printf ("ok\n");
				} else {
				printf ("fail with code %x\n", status);
			}

			printf ("starting again\n");
			
			/* repeated start */
			twStart ();
			twWait ();
			if ((status = TW_STATUS) == 0x10) {
				printf ("ok\n");
				} else {
				printf ("fail with code %x\n", status);
			}
			
			printf ("writing read request\n");

			/* write device address and read bit */
			TWDR = L3GD20 | TW_READ;
			if (TWCR & (1 << TWWC)) {
				printf("write collision\n");
			}
			twFlush ();
			twWait ();
			if ((status = TW_STATUS) == 0x40) {
				printf ("ok\n");
				} else {
				printf ("fail with code %x\n", status);
			}
			
			/* clear twint and wait for response */
			twFlush ();
			twWait ();
			if ((status = TW_STATUS) == 0x58) {
				printf ("ok\n");
				} else {
				printf ("fail with code %x\n", status);
			}
			unsigned char ret = TWDR;
			
			printf ("register %x is %x\n", reg[i], ret);
			
			twStop ();
			
			printf ("done\n");
			
			/* there is no way to tell whether stop has been sent or not, just wait */
			_delay_ms (10);
			} else {
			printf ("fail\n");
		}
	}
}
#endif
	
int main(void) {
	cpuInit ();
	ledInit ();
	twInit ();
	uartInit ();
	
	/* redirect stdout */
	stdout = &mystdout;
	
	printf ("initialization done\n");

	/* disable power-down-mode */
	twStart ();
	twWait ();
	unsigned char status = 0x0;
	
	/* check status code */
	if ((status = TW_STATUS) == TW_START) {
		/* write device address and write bit */
		TWDR = LIS302DL | TW_WRITE;
		if (TWCR & (1 << TWWC)) {
			printf("write collision\n");
		}
		twFlush ();
		twWait ();
		if ((status = TW_STATUS) == TW_MT_SLA_ACK) {
		} else {
			printf ("fail with code %x\n", status);
		}
		
		/* write subaddress (actually i2c data) */
		TWDR = LIS302DL_CTRLREG1;
		if (TWCR & (1 << TWWC)) {
			printf("write collision\n");
		}
		twFlush ();
		twWait ();
		if ((status = TW_STATUS) == TW_MT_DATA_ACK) {
		} else {
			printf ("fail with code %x\n", status);
		}
					
		/* write actual data */
		TWDR = 0b01000111;
		if (TWCR & (1 << TWWC)) {
			printf("write collision\n");
		}
		twFlush ();
		twWait ();
		if ((status = TW_STATUS) == TW_MT_DATA_ACK) {
		} else {
			printf ("fail with code %x\n", status);
		}
		
		twStop ();
		_delay_ms (100);
	} else {
		printf ("write: start failed\n");
	}

	while (1) {
		unsigned char reg[] = {0x29, 0x2b, 0x2d};
		signed char val[6];
		//unsigned char reg[] = {0xf, 0x20, 0x21, 0x22, 0x23, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2d, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f};
		for (unsigned char i = 0; i < sizeof (reg)/sizeof(*reg); i++) {
			twStart ();
			twWait ();
			unsigned char status = 0x0;
		
			/* check status code */
			if ((status = TW_STATUS) == TW_START) {
		
				/* write device address and write bit */
				TWDR = LIS302DL | TW_WRITE;
				if (TWCR & (1 << TWWC)) {
					printf("write collision\n");
				}
				twFlush ();
				twWait ();
				if ((status = TW_STATUS) == TW_MT_SLA_ACK) {
				} else {
					printf ("fail with code %x\n", status);
				}
		
				/* write subaddress (actually i2c data) */
				TWDR = reg[i];
				if (TWCR & (1 << TWWC)) {
					printf("write collision\n");
				}
				twFlush ();
				twWait ();
				if ((status = TW_STATUS) == TW_MT_DATA_ACK) {
				} else {
					printf ("fail with code %x\n", status);
				}
		
				/* repeated start */
				twStart ();
				twWait ();
				if ((status = TW_STATUS) == TW_REP_START) {
				} else {
					printf ("fail with code %x\n", status);
				}
			
				/* write device address and read bit */
				TWDR = LIS302DL | TW_READ;
				if (TWCR & (1 << TWWC)) {
					printf("write collision\n");
				}
				twFlush ();
				twWait ();
				if ((status = TW_STATUS) == TW_MR_SLA_ACK) {
				} else {
					printf ("fail with code %x\n", status);
				}
			
				/* clear twint and wait for response */
				twFlush ();
				twWait ();
				if ((status = TW_STATUS) == TW_MR_DATA_NACK) {
				} else {
					printf ("fail with code %x\n", status);
				}
				unsigned char ret = TWDR;
				val[i] = ret;
			
				twStop ();
	
				/* there is no way to tell whether stop has been sent or not, just wait */
				_delay_ms (10);
			} else {
				printf ("fail with code %x\n", status);
				_delay_ms (1000);
			}
		}
		//printf ("%i/%i/%i\n", (val[1] << 8) | val[0], (val[3] << 8) | val[2], (val[5] << 8) | val[4]);
		printf ("%i/%i/%i\n", val[0], val[1], val[2]);
	}

	while (1);
}
