#ifndef APP_STATE_H
#define APP_STATE_H

#include <stdint.h>
#include <stdbool.h>
#include <termios.h>
#include <signal.h>

#define ANSI_COLOR_RESET "\x1b[0m"
#define ESC_KEY 27
#define MAX_PATH_LENGTH 1024
#define DEFAULT_SCALE_STEP 0.1f
#define DEFAULT_MOVE_STEP 5
#define TARGET_FPS 30
#define FRAME_DELAY (1000000 / TARGET_FPS)

extern volatile sig_atomic_t keep_running;

// Предварительно рассчитанные ANSI коды цветов
extern const char* ansi_colors[6][6][6];
extern char empty_color[];

// Структура для хранения состояния пикселя
typedef struct {
    char ascii_char;
    const char* color_code;
    uint8_t r, g, b;
} PixelState;

typedef struct VideoState VideoState;

typedef struct {
    int target_width;
    int target_height;
    float scale_factor;
    int offset_x;
    int offset_y;
    bool color_mode;
    bool show_info;
    bool show_border;
    bool invert_colors;
    bool dithering;
    bool zoom_effect;
    int zoom_level;
    char current_filename[MAX_PATH_LENGTH];
    struct termios original_termios;
    bool is_video;
    bool pause_video;
    float playback_speed;
    bool audio_enabled;
    
    PixelState* previous_frame;
    PixelState* current_frame;
    bool first_frame;
    
    bool audio_playing;
} AppState;

#endif