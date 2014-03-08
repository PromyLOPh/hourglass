#ifndef GYROSCOPE_H
#define GYROSCOPE_H

#include <stdbool.h>
#include <stdint.h>

void gyroInit ();
void gyroStart ();
bool gyroRead ();
volatile const int16_t *gyroGet ();

#endif /* GYROSCOPE_H */

