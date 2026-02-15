#include "../include/display.h"
#include "../include/terminal.h"  // Добавлено для get_terminal_size
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <signal.h>  // Добавлено

extern volatile sig_atomic_t keep_running;  // Добавлено

const char* ansi_colors[6][6][6] = {0};
char empty_color[] = "";

void determine_scale(AppState* state) {
    int term_width, term_height;
    get_terminal_size(&term_width, &term_height);
    state->target_width = term_width;
    state->target_height = term_height - (state->show_info ? 2 : 1);
    
    // Переинициализируем буферы при изменении размера
    free_frame_buffers(state);
    init_frame_buffers(state);
}

void init_ansi_colors() {
    for (int r = 0; r < 6; r++) {
        for (int g = 0; g < 6; g++) {
            for (int b = 0; b < 6; b++) {
                int ansi_code = 16 + (36 * r) + (6 * g) + b;
                char* color_str = malloc(20);
                snprintf(color_str, 20, "\x1b[38;5;%dm", ansi_code);
                ansi_colors[r][g][b] = color_str;
            }
        }
    }
}

void init_frame_buffers(AppState* state) {
    int buffer_size = state->target_width * state->target_height;
    state->previous_frame = calloc(buffer_size, sizeof(PixelState));
    state->current_frame = calloc(buffer_size, sizeof(PixelState));
    state->first_frame = true;
}

void free_frame_buffers(AppState* state) {
    free(state->previous_frame);
    free(state->current_frame);
}

char get_ascii_char(int brightness) {
    static const char ascii_chars[] = " .'`^\",:;Il!i><~+_-?][}{1)(|\\/tfjrxnuvczXYUJCLQ0OZmwqpdbkhao*#MW&8%B$@";
    return ascii_chars[(brightness * (sizeof(ascii_chars) - 2)) / 255];
}

const char* rgb_to_ansi_fast(const AppState* state, uint8_t r, uint8_t g, uint8_t b) {
    if (!state->color_mode) {
        return empty_color;
    }
    
    if (state->invert_colors) {
        r = 255 - r;
        g = 255 - g;
        b = 255 - b;
    }
    
    int ansi_r = r / 51;
    int ansi_g = g / 51;
    int ansi_b = b / 51;
    
    ansi_r = ansi_r > 5 ? 5 : ansi_r;
    ansi_g = ansi_g > 5 ? 5 : ansi_g;
    ansi_b = ansi_b > 5 ? 5 : ansi_b;
    
    return ansi_colors[ansi_r][ansi_g][ansi_b];
}

void get_resized_pixel(const AppState* state, const uint8_t *image, int original_width, int original_height,
                      int channels, int target_x, int target_y, uint8_t *pixel) {
    float x_ratio = (float)original_width / (float)(state->target_width * state->scale_factor);
    float y_ratio = (float)original_height / (float)(state->target_height * state->scale_factor);

    int src_x = (int)((target_x + state->offset_x) * x_ratio);
    int src_y = (int)((target_y + state->offset_y) * y_ratio);

    if (state->zoom_effect && state->zoom_level > 0) {
        float zoom_factor = powf(2.0f, (float)state->zoom_level);
        src_x = (int)(src_x * zoom_factor) % original_width;
        src_y = (int)(src_y * zoom_factor) % original_height;
        src_x = src_x < 0 ? src_x + original_width : src_x;
        src_y = src_y < 0 ? src_y + original_height : src_y;
    } else {
        src_x = src_x < original_width ? src_x : original_width - 1;
        src_y = src_y < original_height ? src_y : original_height - 1;
    }

    int index = (src_y * original_width + src_x) * channels;
    pixel[0] = image[index];
    pixel[1] = image[index + 1];
    pixel[2] = image[index + 2];
}

void display_image_incremental(AppState* state, const uint8_t *buffer, int original_width, int original_height, int channels) {
    static char *display_buffer = NULL;
    static size_t buffer_size = 0;
    
    int needed_size = state->target_width * state->target_height * 64 + 2048;
    if (!display_buffer || buffer_size < (size_t)needed_size) {
        free(display_buffer);
        display_buffer = malloc(needed_size);
        buffer_size = needed_size;
        if (!display_buffer) {
            fprintf(stderr, "Memory allocation failed for display buffer\n");
            return;
        }
    }
    
    char *ptr = display_buffer;
    
    if (state->first_frame) {
        // Первый кадр - полная перерисовка
        memcpy(ptr, "\033[H", 3);
        ptr += 3;
    }

    int changed_pixels = 0;
    
    for (int y = 0; y < state->target_height; y++) {
        for (int x = 0; x < state->target_width; x++) {
            int index = y * state->target_width + x;
            uint8_t pixel[3];
            get_resized_pixel(state, buffer, original_width, original_height, channels, x, y, pixel);
            
            int brightness = (int)(0.299 * pixel[0] + 0.587 * pixel[1] + 0.114 * pixel[2]);
            char ascii_char = get_ascii_char(brightness);
            const char* color_code = rgb_to_ansi_fast(state, pixel[0], pixel[1], pixel[2]);
            
            // Сохраняем текущее состояние
            state->current_frame[index].ascii_char = ascii_char;
            state->current_frame[index].color_code = color_code;
            state->current_frame[index].r = pixel[0];
            state->current_frame[index].g = pixel[1];
            state->current_frame[index].b = pixel[2];
            
            // Сравниваем с предыдущим кадром
            if (state->first_frame || 
                state->previous_frame[index].ascii_char != ascii_char ||
                state->previous_frame[index].color_code != color_code) {
                
                changed_pixels++;
                
                if (!state->first_frame) {
                    // Перемещаем курсор только если нужно
                    ptr += sprintf(ptr, "\033[%d;%dH", y + 1, x + 1);
                }
                
                // Устанавливаем цвет и выводим символ
                size_t color_len = strlen(color_code);
                if (color_len > 0) {
                    memcpy(ptr, color_code, color_len);
                    ptr += color_len;
                }
                
                *ptr++ = ascii_char;
                
                // Сбрасываем цвет если нужно
                if (color_len > 0) {
                    memcpy(ptr, ANSI_COLOR_RESET, 4);
                    ptr += 4;
                }
            }
        }
    }
    
    if (state->first_frame) {
        state->first_frame = false;
    }
    
    // Обновляем предыдущий кадр
    memcpy(state->previous_frame, state->current_frame, 
           state->target_width * state->target_height * sizeof(PixelState));
    
    // Выводим информацию если нужно
    if (state->show_info && changed_pixels > 0) {
        ptr += sprintf(ptr, "\033[%d;1H", state->target_height + 1);
        ptr += sprintf(ptr, "File: %s | Audio: %s | Scale: %.1fx", 
               state->current_filename, 
               state->audio_playing ? "ON" : "OFF",
               state->scale_factor);
    }
    
    // Единый вывод с обработкой ошибок
    if (ptr > display_buffer) {
        ssize_t written = write(STDOUT_FILENO, display_buffer, ptr - display_buffer);
        if (written == -1) {
            // Обработка ошибки записи
            perror("write error");
        }
    }
}