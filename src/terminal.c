#include "../include/terminal.h"
#include "../include/display.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/select.h>
#include <string.h>
#include <termios.h>
#include <math.h>

volatile sig_atomic_t keep_running = 1;
static volatile sig_atomic_t resize_requested = 0;  // Флаг изменения размера

void handle_winch(int sig) {
    (void)sig;
    resize_requested = 1;
}

void initialize_terminal(AppState* state) {
    tcgetattr(STDIN_FILENO, &state->original_termios);
    struct termios newt = state->original_termios;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGWINCH, handle_winch);  // Добавляем обработчик изменения размера
    
    setvbuf(stdout, NULL, _IOFBF, 65536);
    
    printf("\033[?25l\033[H\033[J");
    fflush(stdout);
}

void restore_terminal(AppState* state) {
    printf("\033[?25h\033[H\033[J");
    fflush(stdout);
    tcsetattr(STDIN_FILENO, TCSANOW, &state->original_termios);
}

void handle_signal(int sig) {
    (void)sig;
    keep_running = 0;
}

void get_terminal_size(int *width, int *height) {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1) {
        *width = 80;
        *height = 24;
    } else {
        *width = w.ws_col;
        *height = w.ws_row;
    }
}

void check_terminal_resize(AppState* state, int* original_width, int* original_height) {
    if (resize_requested) {
        resize_requested = 0;
        
        // Получаем новые размеры
        int new_width, new_height;
        get_terminal_size(&new_width, &new_height);
        
        // Обновляем размеры в состоянии
        state->target_width = new_width;
        state->target_height = new_height - (state->show_info ? 2 : 1);
        
        // Пересчитываем масштаб
        determine_scale(state);
        
        // Полностью очищаем экран и перерисовываем
        printf("\033[2J\033[H");
        fflush(stdout);
        
        // Помечаем, что нужна полная перерисовка
        state->first_frame = true;
        
        // Обновляем оригинальные размеры для расчета смещения
        if (original_width && original_height) {
            int max_offset_x = (int)(*original_width * state->scale_factor) - state->target_width;
            int max_offset_y = (int)(*original_height * state->scale_factor) - state->target_height;
            state->offset_x = fmax(0, fmin(state->offset_x, max_offset_x));
            state->offset_y = fmax(0, fmin(state->offset_y, max_offset_y));
        }
    }
}

void print_help(void) {
    printf("\033[H\033[J");
    printf("\n=== ASCII Video/Image Viewer Help ===\n");
    printf("Navigation:\n");
    printf("  W/↑    - Move up\n");
    printf("  S/↓    - Move down\n");
    printf("  A/←    - Move left\n");
    printf("  D/→    - Move right\n");
    printf("  +      - Zoom in\n");
    printf("  -      - Zoom out\n");
    printf("  Z      - Toggle zoom effect (image tiling)\n");
    printf("\nDisplay Options:\n");
    printf("  C      - Toggle color mode\n");
    printf("  I      - Toggle info display\n");
    printf("  B      - Toggle border\n");
    printf("  N      - Invert colors\n");
    printf("\nAudio Controls:\n");
    printf("  [      - Decrease volume\n");
    printf("  ]      - Increase volume\n");
    printf("\nVideo Controls:\n");
    printf("  SPACE  - Play/Pause video\n");
    printf("  >      - Increase playback speed\n");
    printf("  <      - Decrease playback speed\n");
    printf("  M      - Toggle audio playback\n");
    printf("\nOther:\n");
    printf("  H      - Show this help\n");
    printf("  ESC    - Exit\n");
    printf("\nPress any key to continue...\n");
    getchar();
}

void handle_user_input(AppState* state, int* original_width, int* original_height) {
    struct timeval tv = {0, 1000};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    
    if (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) <= 0) {
        return;
    }

    int ch = getchar();
    
    switch (ch) {
        case ESC_KEY:
            keep_running = 0;
            break;
            
        case 'h': case 'H':
            print_help();
            state->first_frame = true;
            break;
            
        case '+': case '=':
            state->scale_factor += DEFAULT_SCALE_STEP;
            state->offset_x = 0;
            state->offset_y = 0;
            if (state->zoom_effect) {
                state->zoom_level++;
            }
            state->first_frame = true;
            break;
            
        case '-': case '_':
            if (state->scale_factor > DEFAULT_SCALE_STEP) {
                state->scale_factor -= DEFAULT_SCALE_STEP;
                state->offset_x = 0;
                state->offset_y = 0;
                if (state->zoom_effect && state->zoom_level > 0) {
                    state->zoom_level--;
                }
                state->first_frame = true;
            }
            break;
            
        case 'z': case 'Z':
            state->zoom_effect = !state->zoom_effect;
            if (!state->zoom_effect) {
                state->zoom_level = 0;
            }
            state->first_frame = true;
            break;
            
        case 'w': case 'W':
            state->offset_y -= DEFAULT_MOVE_STEP;
            state->first_frame = true;
            break;
            
        case 's': case 'S':
            state->offset_y += DEFAULT_MOVE_STEP;
            state->first_frame = true;
            break;
            
        case 'a': case 'A':
            state->offset_x -= DEFAULT_MOVE_STEP;
            state->first_frame = true;
            break;
            
        case 'd': case 'D':
            state->offset_x += DEFAULT_MOVE_STEP;
            state->first_frame = true;
            break;
            
        case 'c': case 'C':
            state->color_mode = !state->color_mode;
            state->first_frame = true;
            break;
            
        case 'i': case 'I':
            state->show_info = !state->show_info;
            // Пересчитываем высоту при изменении info
            check_terminal_resize(state, original_width, original_height);
            break;
            
        case 'b': case 'B':
            state->show_border = !state->show_border;
            state->first_frame = true;
            break;
            
        case 'n': case 'N':
            state->invert_colors = !state->invert_colors;
            state->first_frame = true;
            break;
            
        case 'm': case 'M':
            state->audio_enabled = !state->audio_enabled;
            break;
            
        case '[': case '{':
            state->audio_volume = fmax(0.3f, state->audio_volume - 0.05f);
            printf("\033[%d;1H\033[K🔉 Volume: %.0f%% (use [ to decrease, ] to increase)\033[%d;1H", 
                   state->target_height + 2, state->audio_volume * 100,
                   state->target_height + 1);
            fflush(stdout);
            state->first_frame = true;
            break;
            
        case ']': case '}':
            state->audio_volume = fmin(1.0f, state->audio_volume + 0.05f);
            printf("\033[%d;1H\033[K🔊 Volume: %.0f%% (use [ to decrease, ] to increase)\033[%d;1H", 
                   state->target_height + 2, state->audio_volume * 100,
                   state->target_height + 1);
            fflush(stdout);
            state->first_frame = true;
            break;
    }
    
    int max_offset_x = (int)(*original_width * state->scale_factor) - state->target_width;
    int max_offset_y = (int)(*original_height * state->scale_factor) - state->target_height;
    
    if (max_offset_x > 0) {
        state->offset_x = fmax(0, fmin(state->offset_x, max_offset_x));
    } else {
        state->offset_x = 0;
    }
    
    if (max_offset_y > 0) {
        state->offset_y = fmax(0, fmin(state->offset_y, max_offset_y));
    } else {
        state->offset_y = 0;
    }
}