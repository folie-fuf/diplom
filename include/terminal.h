#ifndef TERMINAL_H
#define TERMINAL_H

#include "app_state.h"
#include <signal.h>

void initialize_terminal(AppState* state);
void restore_terminal(AppState* state);
void handle_signal(int sig);
void handle_winch(int sig);  // Новая функция для SIGWINCH
void get_terminal_size(int *width, int *height);
void print_help();
void handle_user_input(AppState* state, int* original_width, int* original_height);
void check_terminal_resize(AppState* state, int* original_width, int* original_height);  // Новая функция

#endif