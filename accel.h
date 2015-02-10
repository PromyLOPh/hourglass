#ifndef ACCEL_H
#define ACCEL_H

#include <stdbool.h>
#include <stdint.h>

#define HORIZON_NONE 0
#define HORIZON_POS 1
#define HORIZON_NEG 2

typedef uint8_t horizon;

void accelInit ();
void accelStart ();
void accelProcess ();
int8_t accelGetZ ();
int8_t accelGetNormalizedZ ();
uint8_t accelGetShakeCount ();
void accelResetShakeCount ();
horizon accelGetHorizon ();

#endif /* ACCEL_H */

