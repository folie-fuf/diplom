#include "../include/audio.h"
#include "../include/platform.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <stdio.h>
#include <math.h>
#include <errno.h>

typedef struct {
    pid_t pid;
    struct timespec start_time;
    double start_offset;
    const char* current_file;
    float volume;
} AudioContext;

static AudioContext audio_ctx = {0};
static const char* cached_audio_backend = NULL;

// Глобальная переменная для хранения всех активных PID
#define MAX_AUDIO_PROCS 16
static pid_t active_audio_pids[MAX_AUDIO_PROCS] = {0};
static int active_audio_count = 0;

// Добавляем PID в список
static void add_audio_pid(pid_t pid) {
    if (active_audio_count < MAX_AUDIO_PROCS) {
        active_audio_pids[active_audio_count++] = pid;
    }
}

// Удаляем PID из списка
static void remove_audio_pid(pid_t pid) {
    for (int i = 0; i < active_audio_count; i++) {
        if (active_audio_pids[i] == pid) {
            for (int j = i; j < active_audio_count - 1; j++) {
                active_audio_pids[j] = active_audio_pids[j + 1];
            }
            active_audio_count--;
            break;
        }
    }
}

// Убиваем все аудио процессы
void audio_kill_all(void) {
    // Убиваем текущий процесс
    if (audio_ctx.pid > 0) {
        kill(audio_ctx.pid, SIGTERM);
        usleep(100000);
        kill(audio_ctx.pid, SIGKILL);
        waitpid(audio_ctx.pid, NULL, WNOHANG);
        remove_audio_pid(audio_ctx.pid);
        audio_ctx.pid = 0;
    }
    
    // Убиваем все остальные аудио процессы
    for (int i = 0; i < active_audio_count; i++) {
        if (active_audio_pids[i] > 0) {
            kill(active_audio_pids[i], SIGTERM);
            usleep(100000);
            kill(active_audio_pids[i], SIGKILL);
            waitpid(active_audio_pids[i], NULL, WNOHANG);
        }
    }
    active_audio_count = 0;
}

static bool check_audio_player(const char* player) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "which %s > /dev/null 2>&1", player);
    return system(cmd) == 0;
}

static const char* select_audio_backend_cached(void) {
    if (cached_audio_backend) {
        return cached_audio_backend;
    }
    
    const char* players[] = {"ffplay", "mpv", "mplayer", "vlc", NULL};
    
    for (int i = 0; players[i] != NULL; i++) {
        if (check_audio_player(players[i])) {
            cached_audio_backend = players[i];
            return cached_audio_backend;
        }
    }
    
    return NULL;
}

static bool start_external_player(const char* player, const char* filename, double start_time, float volume) {
    // Останавливаем предыдущий аудио процесс
    if (audio_ctx.pid > 0) {
        kill(audio_ctx.pid, SIGTERM);
        usleep(100000);
        kill(audio_ctx.pid, SIGKILL);
        waitpid(audio_ctx.pid, NULL, WNOHANG);
        remove_audio_pid(audio_ctx.pid);
        audio_ctx.pid = 0;
    }
    
    float safe_volume = fmax(0.3, fmin(1.0, volume));
    if (volume > 0.7) {
        safe_volume = 0.7;
    }
    
    pid_t pid = fork();
    if (pid == 0) {
        // НЕ вызываем setsid() - процесс привязан к родителю
        // При закрытии терминала процесс получит сигнал и умрет
        
        // Игнорируем SIGHUP (чтобы не умереть раньше времени), 
        // но SIGTERM/SIGINT дойдут при закрытии терминала
        signal(SIGHUP, SIG_IGN);
        
        // Перенаправляем stdout/stderr в /dev/null
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull != -1) {
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        
        char start_str[32];
        snprintf(start_str, sizeof(start_str), "%.2f", start_time);
        
        char volume_str[32];
        snprintf(volume_str, sizeof(volume_str), "%.0f", safe_volume * 100);
        
        if (strcmp(player, "ffplay") == 0) {
            char afilter[64];
            snprintf(afilter, sizeof(afilter), "volume=%.2f", safe_volume);
            execlp("ffplay", "ffplay", "-nodisp", "-autoexit", "-loglevel", "quiet",
                   "-af", afilter, "-ss", start_str, filename, NULL);
        } else if (strcmp(player, "mpv") == 0) {
            execlp("mpv", "mpv", "--no-video", "--no-terminal", "--quiet",
                   "--volume", volume_str,
                   "--audio-client-name=ascii_player",
                   "--audio-buffer=0.2",
                   "--audio-exclusive=no",
                   "--start", start_str, filename, NULL);
        } else if (strcmp(player, "mplayer") == 0) {
            execlp("mplayer", "mplayer", "-quiet", "-vo", "null",
                   "-volume", volume_str, "-softvol", "-softvol-max", "100",
                   "-ss", start_str, filename, NULL);
        } else if (strcmp(player, "vlc") == 0) {
            char vlc_volume[32];
            snprintf(vlc_volume, sizeof(vlc_volume), "%d", (int)(safe_volume * 256));
            execlp("cvlc", "cvlc", "--play-and-exit", "--no-video",
                   "--volume", vlc_volume, "--start-time", start_str, filename, NULL);
        }
        
        _exit(1);
    } else if (pid > 0) {
        audio_ctx.pid = pid;
        audio_ctx.start_offset = start_time;
        audio_ctx.current_file = filename;
        audio_ctx.volume = safe_volume;
        clock_gettime(CLOCK_MONOTONIC, &audio_ctx.start_time);
        
        add_audio_pid(pid);
        usleep(200000);
        return true;
    }
    
    return false;
}

bool audio_has_stream(const char* filename) {
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

bool audio_init(AudioSystem* audio, const char* filename) {
    memset(audio, 0, sizeof(AudioSystem));
    
    if (!audio_has_stream(filename)) {
        return false;
    }
    
    const char* backend = select_audio_backend_cached();
    if (backend) {
        if (strcmp(backend, "ffplay") == 0) audio->backend = AUDIO_FFPLAY;
        else if (strcmp(backend, "mpv") == 0) audio->backend = AUDIO_MPV;
        else if (strcmp(backend, "mplayer") == 0) audio->backend = AUDIO_MPLAYER;
        else if (strcmp(backend, "vlc") == 0) audio->backend = AUDIO_VLC;
        
        audio->available = true;
        audio->volume = 0.6f;
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
    
    if (start_external_player(player_name, filename, start_time, audio->volume)) {
        audio->playing = true;
        audio->current_time = start_time;
        return true;
    }
    
    return false;
}

void audio_stop(AudioSystem* audio) {
    if (!audio->playing) return;
    
    if (audio_ctx.pid > 0) {
        kill(audio_ctx.pid, SIGTERM);
        usleep(100000);
        kill(audio_ctx.pid, SIGKILL);
        waitpid(audio_ctx.pid, NULL, WNOHANG);
        remove_audio_pid(audio_ctx.pid);
        audio_ctx.pid = 0;
    }
    
    audio->playing = false;
    audio->current_time = 0;
}

double audio_get_time(AudioSystem* audio) {
    if (!audio->playing || audio_ctx.pid == 0) {
        return audio->current_time;
    }
    
    int status;
    if (waitpid(audio_ctx.pid, &status, WNOHANG) != 0) {
        audio->playing = false;
        remove_audio_pid(audio_ctx.pid);
        audio_ctx.pid = 0;
        return audio->current_time;
    }
    
    struct timespec current_time;
    clock_gettime(CLOCK_MONOTONIC, &current_time);
    
    double elapsed = (double)(current_time.tv_sec - audio_ctx.start_time.tv_sec) +
                    (double)(current_time.tv_nsec - audio_ctx.start_time.tv_nsec) / 1000000000.0;
    
    audio->current_time = audio_ctx.start_offset + elapsed;
    return audio->current_time;
}

void audio_set_volume(AudioSystem* audio, float volume) {
    if (!audio) return;
    
    if (volume < 0.3f) volume = 0.3f;
    if (volume > 1.0f) volume = 1.0f;
    
    audio->volume = volume;
    
    if (audio->playing && audio->platform_data) {
        double current_time = audio_get_time(audio);
        audio_stop(audio);
        audio_play(audio, (const char*)audio->platform_data, current_time);
    }
}

float audio_get_volume(AudioSystem* audio) {
    return audio ? audio->volume : 0.6f;
}

void audio_cleanup(AudioSystem* audio) {
    audio_stop(audio);
    if (audio->platform_data) {
        free(audio->platform_data);
        audio->platform_data = NULL;
    }
    memset(audio, 0, sizeof(AudioSystem));
}

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
        audio->volume = 0.6f;
        audio->system->volume = 0.6f;
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
            sys->volume = audio->volume;
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

void set_audio_volume(AudioState* audio, float volume) {
    if (!audio || !audio->initialized || !audio->system) return;
    
    audio->volume = volume;
    audio_set_volume(audio->system, volume);
}

float get_audio_volume(AudioState* audio) {
    if (!audio || !audio->initialized || !audio->system) return 0.6f;
    return audio->volume;
}