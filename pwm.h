#ifndef PWM_H
#define PWM_H

#include <stdint.h>
void pwmInit ();
void pwmStart ();
void pwmStop ();
void pwmSetBlink (const uint8_t, const uint8_t);

/* LED on (no blink) */
#define PWM_BLINK_ON UINT8_MAX
/* LED off */
#define PWM_BLINK_OFF 0
#define PWM_LED_COUNT 6

#endif /* PWM_H */

