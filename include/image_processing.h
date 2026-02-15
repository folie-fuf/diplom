#ifndef IMAGE_PROCESSING_H
#define IMAGE_PROCESSING_H

#include "app_state.h"
#include <stdio.h>

void process_jpeg(FILE *file, AppState* state);
void apply_dithering(uint8_t* image, int width, int height, int channels);
int get_file_type(const char* filename);

#endif