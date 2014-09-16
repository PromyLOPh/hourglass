#ifndef PWM_H
#define PWM_H

#include <stdint.h>
void pwmInit ();
void pwmStart ();
void pwmStop ();
void pwmSet (const uint8_t, const uint8_t);

typedef enum {
	SPEAKER_BEEP,
} speakerMode;

void speakerStart (const speakerMode);

#define PWM_LED_COUNT 6

#define PWM_OFF 0
/* must be power-of-two */
#define PWM_MAX_BRIGHTNESS 8
#define PWM_ON PWM_MAX_BRIGHTNESS

#endif /* PWM_H */

