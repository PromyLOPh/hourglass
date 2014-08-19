#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

void timerStart (const uint32_t t);
uint32_t timerHit ();
void timerStop ();

#endif /* TIMER_H */

