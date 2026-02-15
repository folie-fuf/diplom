#ifndef DISPLAY_H
#define DISPLAY_H

#include "app_state.h"
#include <stdint.h>  // Добавлено для uint8_t

void determine_scale(AppState* state);
void display_image_incremental(AppState* state, const uint8_t *buffer, 
                               int original_width, int original_height, int channels);
void init_ansi_colors();
void init_frame_buffers(AppState* state);
void free_frame_buffers(AppState* state);
char get_ascii_char(int brightness);
const char* rgb_to_ansi_fast(const AppState* state, uint8_t r, uint8_t g, uint8_t b);
void get_resized_pixel(const AppState* state, const uint8_t *image, int original_width, 
                      int original_height, int channels, int target_x, int target_y, 
                      uint8_t *pixel);

#endif