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
