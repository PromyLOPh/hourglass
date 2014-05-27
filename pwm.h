#ifndef PWM_H
#define PWM_H

#include <stdint.h>
void pwmInit ();
void pwmStart ();
void pwmStop ();
void pwmSetBrightness (const uint8_t i, const uint8_t b);

#endif /* PWM_H */

