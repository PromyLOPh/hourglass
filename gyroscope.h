#ifndef GYROSCOPE_H
#define GYROSCOPE_H

#include <stdbool.h>
#include <stdint.h>

void gyroscopeInit ();
void gyroscopeStart ();
bool gyroscopeRead ();
volatile const int16_t *gyroscopeGet ();

#endif /* GYROSCOPE_H */

