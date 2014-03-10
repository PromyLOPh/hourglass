#ifndef GYROSCOPE_H
#define GYROSCOPE_H

#include <stdbool.h>
#include <stdint.h>

void gyroInit ();
void gyroStart ();
bool gyroProcess ();
void gyroResetAngle ();
const int16_t *gyroGetAngle ();
volatile const int16_t *gyroGetRaw ();

#endif /* GYROSCOPE_H */

