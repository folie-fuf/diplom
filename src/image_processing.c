#include "../include/image_processing.h"
#include "../include/terminal.h"
#include "../include/display.h"
#include <jpeglib.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>

extern volatile sig_atomic_t keep_running;  // Добавлено

void apply_dithering(uint8_t* image, int width, int height, int channels) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = (y * width + x) * channels;
            uint8_t old_r = image[idx];
            uint8_t old_g = channels > 1 ? image[idx + 1] : old_r;
            uint8_t old_b = channels > 2 ? image[idx + 2] : old_r;
            
            uint8_t new_r = old_r < 128 ? 0 : 255;
            uint8_t new_g = old_g < 128 ? 0 : 255;
            uint8_t new_b = old_b < 128 ? 0 : 255;
            
            image[idx] = new_r;
            if (channels > 1) image[idx + 1] = new_g;
            if (channels > 2) image[idx + 2] = new_b;
            
            int err_r = old_r - new_r;
            int err_g = old_g - new_g;
            int err_b = old_b - new_b;
            
            if (x + 1 < width) {
                int idx_right = (y * width + (x + 1)) * channels;
                image[idx_right] += err_r * 7 / 16;
                if (channels > 1) image[idx_right + 1] += err_g * 7 / 16;
                if (channels > 2) image[idx_right + 2] += err_b * 7 / 16;
            }
            
            if (y + 1 < height) {
                if (x - 1 >= 0) {
                    int idx_bottom_left = ((y + 1) * width + (x - 1)) * channels;
                    image[idx_bottom_left] += err_r * 3 / 16;
                    if (channels > 1) image[idx_bottom_left + 1] += err_g * 3 / 16;
                    if (channels > 2) image[idx_bottom_left + 2] += err_b * 3 / 16;
                }
                
                int idx_bottom = ((y + 1) * width + x) * channels;
                image[idx_bottom] += err_r * 5 / 16;
                if (channels > 1) image[idx_bottom + 1] += err_g * 5 / 16;
                if (channels > 2) image[idx_bottom + 2] += err_b * 5 / 16;
                
                if (x + 1 < width) {
                    int idx_bottom_right = ((y + 1) * width + (x + 1)) * channels;
                    image[idx_bottom_right] += err_r * 1 / 16;
                    if (channels > 1) image[idx_bottom_right + 1] += err_g * 1 / 16;
                    if (channels > 2) image[idx_bottom_right + 2] += err_b * 1 / 16;
                }
            }
        }
    }
}

void process_jpeg(FILE *file, AppState* state) {
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, file);
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);

    int original_width = cinfo.output_width;
    int original_height = cinfo.output_height;
    int channels = cinfo.output_components;

    uint8_t *buffer = malloc(original_width * original_height * channels);
    if (!buffer) {
        fprintf(stderr, "Memory allocation failed\n");
        jpeg_destroy_decompress(&cinfo);
        return;
    }

    uint8_t *row = buffer;
    while (cinfo.output_scanline < cinfo.output_height) {
        jpeg_read_scanlines(&cinfo, &row, 1);
        row += original_width * channels;
    }

    if (state->dithering) {
        apply_dithering(buffer, original_width, original_height, channels);
    }

    while (keep_running) {
        display_image_incremental(state, buffer, original_width, original_height, channels);
        handle_user_input(state, &original_width, &original_height);
    }

    free(buffer);
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
}

int get_file_type(const char* filename) {
    const char *ext = strrchr(filename, '.');
    if (!ext) return -1;
    
    ext++;
    
    // Конвертируем в нижний регистр для сравнения
    char ext_lower[16];
    int i = 0;
    while (ext[i] && i < 15) {
        ext_lower[i] = tolower((unsigned char)ext[i]);
        i++;
    }
    ext_lower[i] = '\0';
    
    if (strcmp(ext_lower, "jpg") == 0 || strcmp(ext_lower, "jpeg") == 0) return 1;
    if (strcmp(ext_lower, "mp4") == 0 || strcmp(ext_lower, "avi") == 0 || 
        strcmp(ext_lower, "mkv") == 0 || strcmp(ext_lower, "mov") == 0 ||
        strcmp(ext_lower, "webm") == 0) return 5;
    
    return -1;
}