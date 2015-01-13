#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>
#include <stdbool.h>

void timerStart (const uint32_t t, const bool);
uint32_t timerHit ();
void timerStop ();

#endif /* TIMER_H */

