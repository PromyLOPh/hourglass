#ifndef GYROSCOPE_H
#define GYROSCOPE_H

#include <stdbool.h>
#include <stdint.h>

void gyroInit ();
void gyroStart ();
bool gyroProcess ();
void gyroResetAccum ();
int32_t gyroGetZAccum ();
int16_t gyroGetZRaw ();
int8_t gyroGetZTicks ();

#endif /* GYROSCOPE_H */

