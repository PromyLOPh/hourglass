#ifndef PWM_H
#define PWM_H

#include <stdint.h>
void pwmInit ();
void pwmStart ();
void pwmStop ();
void pwmSet (const uint8_t, const uint8_t);

/* LED on (max brightness) */
#define PWM_ON UINT8_MAX
/* LED off */
#define PWM_OFF 0
#define PWM_LED_COUNT 6
#define PWM_MAX_BRIGHTNESS 8

#endif /* PWM_H */

