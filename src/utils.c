#include "../include/utils.h"
#include <math.h>
#include <time.h>

void precise_usleep(long microseconds) {
    struct timespec req = {
        .tv_sec = microseconds / 1000000,
        .tv_nsec = (microseconds % 1000000) * 1000
    };
    nanosleep(&req, NULL);
}

double fmin(double a, double b) {
    return a < b ? a : b;
}

double fmax(double a, double b) {
    return a > b ? a : b;
}

double fabs(double x) {
    return x < 0 ? -x : x;
}