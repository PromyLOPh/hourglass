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
horizon accelGetHorizon (bool * const);
void accelResetShakeCount ();
uint8_t accelGetShakeCount ();

#endif /* ACCEL_H */

