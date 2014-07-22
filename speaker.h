#ifndef SPEAKER_H
#define SPEAKER_H

typedef enum {
	SPEAKER_BEEP,
} speakerMode;

void speakerInit ();
void speakerStart (const speakerMode);
void speakerStop ();

#endif /* SPEAKER_H */

