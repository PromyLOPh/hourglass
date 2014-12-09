#ifndef ACCEL_H
#define ACCEL_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {HORIZON_NONE, HORIZON_POS, HORIZON_NEG} horizon;

void accelInit ();
void accelStart ();
bool accelProcess ();
int8_t accelGetZ ();
int8_t accelGetNormalizedZ ();
uint8_t accelGetShakeCount ();
void accelResetShakeCount ();
horizon accelGetHorizon ();

#endif /* ACCEL_H */

