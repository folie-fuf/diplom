#include "../include/utils.h"
#include <time.h>

void precise_usleep(long microseconds) {
    struct timespec req = {
        .tv_sec = microseconds / 1000000,
        .tv_nsec = (microseconds % 1000000) * 1000
    };
    nanosleep(&req, NULL);
}