#include "beep.h"

#include <SDL3/SDL.h>
#include <math.h>

#define BEEP_DURATION_MS 200
#define BEEP_FREQ 750

static SDL_AudioStream* stream = NULL;

void beep_init(void) {
	SDL_AudioSpec wantSpec = {
		.format   = SDL_AUDIO_F32,   // 32-bit float samples
		.channels = 1,               // mono
		.freq     = 44100            // sample rate
    	};

	stream = SDL_OpenAudioDeviceStream(
		SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, 
		&wantSpec, NULL, 0
    	);

	SDL_ResumeAudioStreamDevice(stream);
}

void beep_play(void) {
	int sr = 44100;
	int samples = (BEEP_DURATION_MS * sr) / 1000;
	float *buf = SDL_malloc(sizeof(float) * samples);

	for (int i = 0; i < samples; i++) {
		float t = (float)i / sr;
		buf[i] = sinf(2.0f * 3.14159265f * BEEP_FREQ * t);
	}

	SDL_PutAudioStreamData(stream, buf, samples * sizeof(float));
	SDL_free(buf);
}

void beep_quit(void) {
	if (stream) {
		SDL_DestroyAudioStream(stream);
		stream = NULL;
    	}
}