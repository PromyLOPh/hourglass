#ifndef GYROSCOPE_H
#define GYROSCOPE_H

#include <stdbool.h>
#include <stdint.h>

void gyroInit ();
void gyroStart ();
bool gyroProcess ();
void gyroResetAccum ();
const int32_t gyroGetZAccum ();
volatile const int16_t gyroGetZRaw ();
const int8_t gyroGetZTicks ();

#endif /* GYROSCOPE_H */

