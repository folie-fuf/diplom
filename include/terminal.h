#ifndef TERMINAL_H
#define TERMINAL_H

#include "app_state.h"
#include <signal.h>  // Добавлено для sig_atomic_t

void initialize_terminal(AppState* state);
void restore_terminal(AppState* state);
void handle_signal(int sig);
void get_terminal_size(int *width, int *height);
void print_help();
void handle_user_input(AppState* state, int* original_width, int* original_height);

#endif