#ifndef AUDIO_H
#define AUDIO_H

#include "app_state.h"
#include <stdbool.h>

// Аудио бэкенды
typedef enum {
    AUDIO_NONE = 0,
    AUDIO_FFPLAY,
    AUDIO_MPV,
    AUDIO_MPLAYER,
    AUDIO_VLC,
    AUDIO_SDL2,
    AUDIO_DIRECT_SOUND  // Для Windows
} AudioBackend;

// Универсальная структура аудио
typedef struct AudioSystem {
    AudioBackend backend;
    bool playing;
    bool available;
    double current_time;
    void* platform_data; // Платформо-специфичные данные
} AudioSystem;

// Предварительное объявление AudioState для video_processing
typedef struct AudioState {
    bool initialized;
    bool playing;
    double current_time;
    AudioSystem* system;
} AudioState;

// Инициализация аудио системы
bool audio_init(AudioSystem* audio, const char* filename);
// Запуск воспроизведения
bool audio_play(AudioSystem* audio, const char* filename, double start_time);
// Остановка воспроизведения
void audio_stop(AudioSystem* audio);
// Пауза/возобновление
void audio_pause(AudioSystem* audio, bool pause);
// Получение текущего времени
double audio_get_time(AudioSystem* audio);
// Проверка наличия аудио в файле
bool audio_has_stream(const char* filename);
// Освобождение ресурсов
void audio_cleanup(AudioSystem* audio);

// Функции для video_processing
bool has_audio_stream(const char* filename);
bool init_audio(AudioState* audio, const char* filename);
void cleanup_audio(AudioState* audio);
void start_audio(AudioState* audio);
void stop_audio(AudioState* audio);
double get_audio_time(AudioState* audio);
void toggle_audio(AudioState* audio);

#endif