#ifndef TW_H
#define TW_H

typedef enum {
	TWM_INVALID = 0,
	TWM_WRITE,
	TWM_READ,
} twMode;

typedef enum {
	TWST_WAIT = 0,
	TWST_OK = 1,
	TWST_ERR = 2,
} twStatus;

#include <stdint.h>

typedef struct {
	twMode mode;
	uint8_t address;
	uint8_t subaddress;
	uint8_t step;
	/* pointer to read/write data */
	uint8_t *data;
	/* number of bytes to be read/written */
	uint8_t count;
	/* current byte */
	uint8_t i;
	twStatus status;
} twReq;

extern volatile twReq twr;

/* i2c device addresses */
#define LIS302DL 0b00111000
#define LIS302DL_WHOAMI 0xf
#define LIS302DL_CTRLREG1 0x20

#include <stdbool.h>

void twInit ();
bool twRequest (const twMode mode, const uint8_t address,
		const uint8_t subaddress, uint8_t * const data, const uint8_t count);

#endif /* TW_H */
