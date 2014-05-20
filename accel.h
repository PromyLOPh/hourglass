#ifndef ACCEL_H
#define ACCEL_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {HORIZON_NONE, HORIZON_POS, HORIZON_NEG} horizon;

void accelInit ();
void accelStart ();
bool accelProcess ();
const int8_t accelGetZ ();
const uint8_t accelGetShakeCount ();
const horizon accelGetHorizon ();

#endif /* ACCEL_H */

