#ifndef ACCEL_H
#define ACCEL_H

#include <stdbool.h>
#include <stdint.h>

void accelInit ();
void accelStart ();
bool accelProcess ();
volatile const int8_t *accelGet ();

#endif /* ACCEL_H */

