#include "../include/platform.h"
#include <string.h>

const char* get_platform_name(void) {
#if OS_WINDOWS
    return "Windows";
#elif OS_LINUX
    return "Linux";
#elif OS_MACOS
    return "macOS";
#elif OS_WSL
    return "WSL (Windows Subsystem for Linux)";
#else
    return "Unknown";
#endif
}

const char* get_audio_backend_name(void) {
    switch (AUDIO_BACKEND) {
        case AUDIO_FFPLAY: return "FFplay";
        case AUDIO_MPV: return "MPV";
        case AUDIO_MPLAYER: return "MPlayer";
        case AUDIO_VLC: return "VLC";
        case AUDIO_SDL2: return "SDL2";
        default: return "None";
    }
}

int check_audio_support(void) {
    // Проверяем доступность аудио
#if OS_WSL
    printf("⚠ WSL обнаружен. Аудио может требовать дополнительной настройки.\n");
    printf("   Рекомендуется настроить PulseAudio на Windows.\n");
#endif
    
    return AUDIO_BACKEND != 0;
}