#include "../include/display.h"
#include "../include/terminal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

extern volatile sig_atomic_t keep_running;

const char* ansi_colors[6][6][6] = {0};
char empty_color[] = "";

#define DISPLAY_BUFFER_SIZE (1024 * 1024)
static char display_buffer[DISPLAY_BUFFER_SIZE];
static size_t buffer_pos = 0;

static void flush_buffer(void) {
    if (buffer_pos > 0) {
        ssize_t result = write(STDOUT_FILENO, display_buffer, buffer_pos);
        (void)result; // Подавляем предупреждение о неиспользуемом результате
        buffer_pos = 0;
    }
}

static void buffer_append(const char* str, size_t len) {
    if (buffer_pos + len >= DISPLAY_BUFFER_SIZE) {
        flush_buffer();
    }
    memcpy(display_buffer + buffer_pos, str, len);
    buffer_pos += len;
}

static void buffer_append_str(const char* str) {
    buffer_append(str, strlen(str));
}

void determine_scale(AppState* state) {
    int term_width, term_height;
    get_terminal_size(&term_width, &term_height);
    state->target_width = term_width;
    state->target_height = term_height - (state->show_info ? 2 : 1);
    
    free_frame_buffers(state);
    init_frame_buffers(state);
}

void init_ansi_colors(void) {
    static int initialized = 0;
    if (initialized) return;
    
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
    initialized = 1;
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
    state->previous_frame = NULL;
    state->current_frame = NULL;
}

char get_ascii_char(int brightness) {
    static const char ascii_chars[] = " .'`^\",:;Il!i><~+_-?][}{1)(|\\/tfjrxnuvczXYUJCLQ0OZmwqpdbkhao*#MW&8%B$@";
    int index = (brightness * (sizeof(ascii_chars) - 2)) / 255;
    return ascii_chars[index];
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
    pixel[1] = channels > 1 ? image[index + 1] : image[index];
    pixel[2] = channels > 2 ? image[index + 2] : image[index];
}

void display_image_incremental(AppState* state, const uint8_t *buffer, 
                               int original_width, int original_height, int channels) {
    char temp_buffer[256]; // Увеличил размер буфера с 64 до 256
    
    if (state->first_frame) {
        buffer_append_str("\033[H");
    }

    for (int y = 0; y < state->target_height; y++) {
        for (int x = 0; x < state->target_width; x++) {
            int index = y * state->target_width + x;
            uint8_t pixel[3];
            get_resized_pixel(state, buffer, original_width, original_height, channels, x, y, pixel);
            
            int brightness = (int)(0.299 * pixel[0] + 0.587 * pixel[1] + 0.114 * pixel[2]);
            char ascii_char = get_ascii_char(brightness);
            const char* color_code = rgb_to_ansi_fast(state, pixel[0], pixel[1], pixel[2]);
            
            state->current_frame[index].ascii_char = ascii_char;
            state->current_frame[index].color_code = color_code;
            state->current_frame[index].r = pixel[0];
            state->current_frame[index].g = pixel[1];
            state->current_frame[index].b = pixel[2];
            
            if (state->first_frame || 
                state->previous_frame[index].ascii_char != ascii_char ||
                state->previous_frame[index].color_code != color_code) {
                
                if (!state->first_frame) {
                    snprintf(temp_buffer, sizeof(temp_buffer), "\033[%d;%dH", y + 1, x + 1);
                    buffer_append_str(temp_buffer);
                }
                
                if (color_code[0] != '\0') {
                    buffer_append_str(color_code);
                }
                
                buffer_append(&ascii_char, 1);
                
                if (color_code[0] != '\0') {
                    buffer_append_str(ANSI_COLOR_RESET);
                }
            }
        }
    }
    
    if (state->first_frame) {
        state->first_frame = false;
    }
    
    memcpy(state->previous_frame, state->current_frame, 
           state->target_width * state->target_height * sizeof(PixelState));
    
    if (state->show_info) {
        snprintf(temp_buffer, sizeof(temp_buffer), "\033[%d;1H", state->target_height + 1);
        buffer_append_str(temp_buffer);
        
        // Используем более безопасное форматирование с ограничением длины имени файла
        char filename_display[128];
        const char* filename = state->current_filename;
        size_t len = strlen(filename);
        if (len > 60) {
            // Обрезаем длинное имя файла
            snprintf(filename_display, sizeof(filename_display), "...%s", filename + len - 57);
        } else {
            strncpy(filename_display, filename, sizeof(filename_display) - 1);
            filename_display[sizeof(filename_display) - 1] = '\0';
        }
        
        snprintf(temp_buffer, sizeof(temp_buffer), "File: %s | Audio: %s | Scale: %.1fx", 
               filename_display, 
               state->audio_playing ? "ON" : "OFF",
               (double)state->scale_factor);
        buffer_append_str(temp_buffer);
    }
    
    flush_buffer();
}