#include "../include/app_state.h"
#include "../include/terminal.h"
#include "../include/display.h"
#include "../include/image_processing.h"
#include "../include/video_processing.h"
#include "../include/audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

static void emergency_cleanup(void) {
    audio_kill_all();
}

static void signal_handler(int sig) {
    (void)sig;
    emergency_cleanup();
    exit(0);
}

int main(int argc, char *argv[]) {
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    signal(SIGHUP, signal_handler);
    
    atexit(emergency_cleanup);
    
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <image/video> [-d] [-c] [-b] [-m]\n", argv[0]);
        fprintf(stderr, "Options:\n");
        fprintf(stderr, "  -d  Enable dithering\n");
        fprintf(stderr, "  -c  Disable color mode\n");
        fprintf(stderr, "  -b  Enable border\n");
        fprintf(stderr, "  -m  Disable audio by default\n");
        return EXIT_FAILURE;
    }

    AppState state = {0};
    
    state.scale_factor = 1.0f;
    state.color_mode = true;
    state.show_info = true;
    state.show_border = false;
    state.invert_colors = false;
    state.dithering = false;
    state.zoom_effect = false;
    state.zoom_level = 0;
    state.is_video = false;
    state.pause_video = false;
    state.playback_speed = 1.0f;
    state.audio_enabled = true;
    state.first_frame = true;
    state.audio_playing = false;
    state.audio_volume = 0.6f;
    
    strncpy(state.current_filename, argv[1], MAX_PATH_LENGTH - 1);
    state.current_filename[MAX_PATH_LENGTH - 1] = '\0';

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) state.dithering = true;
        if (strcmp(argv[i], "-c") == 0) state.color_mode = false;
        if (strcmp(argv[i], "-b") == 0) state.show_border = true;
        if (strcmp(argv[i], "-m") == 0) state.audio_enabled = false;
    }

    determine_scale(&state);
    initialize_terminal(&state);
    init_ansi_colors();

    int file_type = get_file_type(argv[1]);
    if (file_type == 5) {
        process_video(argv[1], &state);
    } else if (file_type == 1) {
        FILE *file = fopen(argv[1], "rb");
        if (file) {
            process_jpeg(file, &state);
            fclose(file);
        } else {
            perror("Unable to open file");
        }
    } else {
        fprintf(stderr, "Unsupported file format\n");
    }

    restore_terminal(&state);
    free_frame_buffers(&state);
    
    emergency_cleanup();
    
    return EXIT_SUCCESS;
}