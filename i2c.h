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

#ifndef TW_H
#define TW_H

#include <stdint.h>

typedef uint8_t twMode;
#define TWM_INVALID 0
#define TWM_WRITE 1
#define TWM_READ 2

typedef uint8_t twStatus;
#define TWST_WAIT 0
#define TWST_OK 1
#define TWST_ERR 2

#include <stdint.h>

typedef struct {
	twMode mode;
	uint8_t address;
	uint8_t subaddress;
	volatile uint8_t step;
	/* pointer to read/write data */
	volatile uint8_t *data;
	/* number of bytes to be read/written */
	uint8_t count;
	/* current byte */
	volatile uint8_t i;
	volatile twStatus status;
	/* i2c bus status at the time if an error occured */
	volatile uint8_t error;
} twReq;

extern twReq twr;

#include <stdbool.h>

void twInit ();
bool twRequest (const twMode mode, const uint8_t address,
		const uint8_t subaddress, uint8_t * const data, const uint8_t count);

#endif /* TW_H */
