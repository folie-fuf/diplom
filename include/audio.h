#ifndef AUDIO_H
#define AUDIO_H

#include "app_state.h"
#include <stdbool.h>

typedef enum {
    AUDIO_NONE = 0,
    AUDIO_FFPLAY,
    AUDIO_MPV,
    AUDIO_MPLAYER,
    AUDIO_VLC,
    AUDIO_SDL2
} AudioBackend;

typedef struct AudioSystem {
    AudioBackend backend;
    bool playing;
    bool available;
    double current_time;
    void* platform_data;
} AudioSystem;

typedef struct AudioState {
    bool initialized;
    bool playing;
    double current_time;
    AudioSystem* system;
} AudioState;

bool audio_init(AudioSystem* audio, const char* filename);
bool audio_play(AudioSystem* audio, const char* filename, double start_time);
void audio_stop(AudioSystem* audio);
double audio_get_time(AudioSystem* audio);
bool audio_has_stream(const char* filename);
void audio_cleanup(AudioSystem* audio);

bool has_audio_stream(const char* filename);
bool init_audio(AudioState* audio, const char* filename);
void cleanup_audio(AudioState* audio);
void start_audio(AudioState* audio);
void stop_audio(AudioState* audio);
double get_audio_time(AudioState* audio);
void toggle_audio(AudioState* audio);

#endif