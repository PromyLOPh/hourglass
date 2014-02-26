#ifndef TW_H
#define TW_H

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

#include <stdint.h>

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

extern volatile twReq twr;

/* i2c device addresses */
#define LIS302DL 0b00111000
#define LIS302DL_WHOAMI 0xf
#define LIS302DL_CTRLREG1 0x20

#include <stdbool.h>

void twInit ();
bool twWrite (const uint8_t address, const uint8_t subaddress,
		const uint8_t data);
bool twReadMulti (const uint8_t address, const uint8_t subaddress,
		uint8_t * const retData, const uint8_t count);

#endif /* TW_H */
