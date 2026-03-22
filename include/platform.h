#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdbool.h>

// Определение операционной системы
#if defined(_WIN32) || defined(_WIN64)
    #define OS_WINDOWS 1
    #define OS_LINUX 0
    #define OS_MACOS 0
    #define OS_WSL 0
#elif defined(__APPLE__) && defined(__MACH__)
    #define OS_WINDOWS 0
    #define OS_LINUX 0
    #define OS_MACOS 1
    #define OS_WSL 0
#elif defined(__linux__)
    #ifdef __WSL__
        #define OS_WINDOWS 0
        #define OS_LINUX 0
        #define OS_MACOS 0
        #define OS_WSL 1
    #else
        #define OS_WINDOWS 0
        #define OS_LINUX 1
        #define OS_MACOS 0
        #define OS_WSL 0
    #endif
#else
    #define OS_WINDOWS 0
    #define OS_LINUX 0
    #define OS_MACOS 0
    #define OS_WSL 0
#endif

// Аудио бэкенды
#define AUDIO_FFPLAY 1
#define AUDIO_MPV 2
#define AUDIO_MPLAYER 3
#define AUDIO_VLC 4
#define AUDIO_SDL2 5

#ifndef AUDIO_BACKEND
    #define AUDIO_BACKEND 0
#endif

const char* get_platform_name(void);
const char* get_audio_backend_name(void);
int check_audio_support(void);

#endif