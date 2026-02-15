#include "../include/terminal.h"
#include "../include/utils.h"  // Добавлено для fmin/fmax
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/select.h>
#include <string.h>
#include <termios.h>

volatile sig_atomic_t keep_running = 1;

void initialize_terminal(AppState* state) {
    tcgetattr(STDIN_FILENO, &state->original_termios);
    struct termios newt = state->original_termios;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    
    setvbuf(stdout, NULL, _IOFBF, 65536);
    
    // Скрыть курсор и очистить экран
    printf("\033[?25l\033[H\033[J");
    fflush(stdout);
}

void restore_terminal(AppState* state) {
    // Показать курсор и очистить экран
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

void print_help() {
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
            state->first_frame = true;
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
            // Аудио управление обрабатывается в video_processing.c
            break;
    }
    
    int max_offset_x = *original_width * state->scale_factor - state->target_width;
    int max_offset_y = *original_height * state->scale_factor - state->target_height;
    
    // Используем функции из utils.h
    state->offset_x = fmax(0, fmin(state->offset_x, max_offset_x));
    state->offset_y = fmax(0, fmin(state->offset_y, max_offset_y));
}