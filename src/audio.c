#include "../include/audio.h"
#include "../include/platform.h"
#include "../include/utils.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

// Общие данные
static struct {
    pid_t pid;
    struct timespec start_time;
    double start_offset;
    const char* current_file;
} audio_ctx = {0};

// Проверка доступности аудио плеера
static bool check_audio_player(const char* player) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "which %s > /dev/null 2>&1", player);
    return system(cmd) == 0;
}

// Выбор лучшего доступного аудио плеера
static const char* select_audio_backend(void) {
    const char* players[] = {"ffplay", "mpv", "mplayer", "vlc", NULL};
    
    for (int i = 0; players[i] != NULL; i++) {
        if (check_audio_player(players[i])) {
            return players[i];
        }
    }
    
    return NULL;
}

// Запуск внешнего аудио плеера
static bool start_external_player(const char* player, const char* filename, double start_time) {
    // Останавливаем предыдущий процесс если есть
    if (audio_ctx.pid > 0) {
        kill(-audio_ctx.pid, SIGTERM);
        waitpid(audio_ctx.pid, NULL, 0);
        audio_ctx.pid = 0;
    }
    
    pid_t pid = fork();
    if (pid == 0) {
        // Дочерний процесс
        setsid();
        
        // Перенаправляем вывод в /dev/null
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull != -1) {
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        
        if (strcmp(player, "ffplay") == 0) {
            char start_str[32];
            snprintf(start_str, sizeof(start_str), "%.2f", start_time);
            execlp("ffplay", "ffplay", "-nodisp", "-autoexit", "-loglevel", "quiet",
                   "-ss", start_str, filename, NULL);
        } else if (strcmp(player, "mpv") == 0) {
            char start_str[32];
            snprintf(start_str, sizeof(start_str), "%.2f", start_time);
            execlp("mpv", "mpv", "--no-video", "--no-terminal", "--quiet",
                   "--start", start_str, filename, NULL);
        } else if (strcmp(player, "mplayer") == 0) {
            char start_str[32];
            snprintf(start_str, sizeof(start_str), "%.2f", start_time);
            execlp("mplayer", "mplayer", "-quiet", "-vo", "null",
                   "-ss", start_str, filename, NULL);
        } else if (strcmp(player, "vlc") == 0) {
            char start_str[32];
            snprintf(start_str, sizeof(start_str), "%.2f", start_time);
            execlp("cvlc", "cvlc", "--play-and-exit", "--no-video", 
                   "--start-time", start_str, filename, NULL);
        }
        
        // Если execlp вернулся, значит ошибка
        _exit(1);
    } else if (pid > 0) {
        audio_ctx.pid = pid;
        audio_ctx.start_offset = start_time;
        audio_ctx.current_file = filename;
        clock_gettime(CLOCK_MONOTONIC, &audio_ctx.start_time);
        usleep(200000); // Даем время на запуск
        return true;
    }
    
    return false;
}

// Проверка наличия аудио потока
bool audio_has_stream(const char* filename) {
    // Простая проверка по расширению
    const char* ext = strrchr(filename, '.');
    if (!ext) return false;
    
    ext++;
    char ext_lower[16];
    int i = 0;
    for (; ext[i] && i < 15; i++) {
        ext_lower[i] = tolower((unsigned char)ext[i]);
    }
    ext_lower[i] = '\0';
    
    const char* audio_formats[] = {
        "mp4", "avi", "mkv", "mov", "webm",
        "mp3", "wav", "flac", "ogg", "m4a", NULL
    };
    
    for (int j = 0; audio_formats[j] != NULL; j++) {
        if (strcmp(ext_lower, audio_formats[j]) == 0) {
            return true;
        }
    }
    
    return false;
}

// Основные функции аудио
bool audio_init(AudioSystem* audio, const char* filename) {
    memset(audio, 0, sizeof(AudioSystem));
    
    // Проверяем наличие аудио в файле
    if (!audio_has_stream(filename)) {
        return false;
    }
    
    // Выбираем лучший доступный бэкенд
    const char* backend = select_audio_backend();
    if (backend) {
        if (strcmp(backend, "ffplay") == 0) {
            audio->backend = AUDIO_FFPLAY;
        } else if (strcmp(backend, "mpv") == 0) {
            audio->backend = AUDIO_MPV;
        } else if (strcmp(backend, "mplayer") == 0) {
            audio->backend = AUDIO_MPLAYER;
        } else if (strcmp(backend, "vlc") == 0) {
            audio->backend = AUDIO_VLC;
        }
        
        audio->available = true;
        // Сохраняем имя файла для последующего использования
        audio->platform_data = strdup(filename);
        return true;
    }
    
    audio->available = false;
    return false;
}

bool audio_play(AudioSystem* audio, const char* filename, double start_time) {
    if (!audio->available || !filename) return false;
    
    const char* player_name = NULL;
    switch (audio->backend) {
        case AUDIO_FFPLAY: player_name = "ffplay"; break;
        case AUDIO_MPV: player_name = "mpv"; break;
        case AUDIO_MPLAYER: player_name = "mplayer"; break;
        case AUDIO_VLC: player_name = "vlc"; break;
        default: return false;
    }
    
    if (start_external_player(player_name, filename, start_time)) {
        audio->playing = true;
        audio->current_time = start_time;
        return true;
    }
    
    return false;
}

void audio_stop(AudioSystem* audio) {
    if (!audio->playing) return;
    
    if (audio_ctx.pid > 0) {
        kill(-audio_ctx.pid, SIGTERM);
        int status;
        waitpid(audio_ctx.pid, &status, 0);
        audio_ctx.pid = 0;
    }
    
    audio->playing = false;
}

double audio_get_time(AudioSystem* audio) {
    if (!audio->playing || audio_ctx.pid == 0) {
        return audio->current_time;
    }
    
    // Проверяем, жив ли процесс
    int status;
    if (waitpid(audio_ctx.pid, &status, WNOHANG) != 0) {
        audio->playing = false;
        audio_ctx.pid = 0;
        return audio->current_time;
    }
    
    // Рассчитываем прошедшее время
    struct timespec current_time;
    clock_gettime(CLOCK_MONOTONIC, &current_time);
    
    double elapsed = (double)(current_time.tv_sec - audio_ctx.start_time.tv_sec) +
                    (double)(current_time.tv_nsec - audio_ctx.start_time.tv_nsec) / 1000000000.0;
    
    audio->current_time = audio_ctx.start_offset + elapsed;
    
    return audio->current_time;
}

void audio_cleanup(AudioSystem* audio) {
    audio_stop(audio);
    if (audio->platform_data) {
        free(audio->platform_data);
        audio->platform_data = NULL;
    }
    memset(audio, 0, sizeof(AudioSystem));
}

// Функции для video_processing
bool has_audio_stream(const char* filename) {
    return audio_has_stream(filename);
}

bool init_audio(AudioState* audio, const char* filename) {
    if (!audio) return false;
    
    memset(audio, 0, sizeof(AudioState));
    
    audio->system = malloc(sizeof(AudioSystem));
    if (!audio->system) return false;
    
    if (audio_init(audio->system, filename)) {
        audio->initialized = true;
        return true;
    }
    
    free(audio->system);
    audio->system = NULL;
    return false;
}

void cleanup_audio(AudioState* audio) {
    if (!audio || !audio->initialized) return;
    
    if (audio->system) {
        audio_cleanup(audio->system);
        free(audio->system);
    }
    
    memset(audio, 0, sizeof(AudioState));
}

void start_audio(AudioState* audio) {
    if (!audio || !audio->initialized || !audio->system) return;
    
    if (!audio->playing) {
        AudioSystem* sys = audio->system;
        if (sys->platform_data) {
            if (audio_play(sys, (const char*)sys->platform_data, audio->current_time)) {
                audio->playing = true;
            }
        }
    }
}

void stop_audio(AudioState* audio) {
    if (!audio || !audio->initialized || !audio->system) return;
    
    if (audio->playing) {
        AudioSystem* sys = audio->system;
        audio_stop(sys);
        audio->playing = false;
    }
}

double get_audio_time(AudioState* audio) {
    if (!audio || !audio->initialized || !audio->system) return 0.0;
    
    if (audio->playing) {
        AudioSystem* sys = audio->system;
        audio->current_time = audio_get_time(sys);
    }
    
    return audio->current_time;
}

void toggle_audio(AudioState* audio) {
    if (!audio || !audio->initialized) return;
    
    if (audio->playing) {
        stop_audio(audio);
    } else {
        start_audio(audio);
    }
}