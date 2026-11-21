#include <stdio.h>
#include <time.h>
int main(void) {
    struct timespec a, b;
    clock_gettime(CLOCK_MONOTONIC, &a);
    double total = 0;
    for (double i = 0; i < 1000000; i++) total += i;
    clock_gettime(CLOCK_MONOTONIC, &b);
    printf("sum: %.0f  loop: %.3f ms\n", total, (b.tv_sec - a.tv_sec) * 1e3 + (b.tv_nsec - a.tv_nsec) / 1e6);
}
