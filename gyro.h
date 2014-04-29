#ifndef GYROSCOPE_H
#define GYROSCOPE_H

#include <stdbool.h>
#include <stdint.h>

void gyroInit ();
void gyroStart ();
bool gyroProcess ();
void gyroResetAccum ();
const int32_t *gyroGetAccum ();
volatile const int16_t *gyroGetRaw ();
const int8_t gyroGetZTicks ();

#endif /* GYROSCOPE_H */

