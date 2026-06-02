#include "lab.h"
#include "user.h"

void lab_u64(char* key, uint64 value) {
    printf("LAB %s=%lu\n", key, value);
}

void lab_i64(char* key, int value) {
    printf("LAB %s=%d\n", key, value);
}

void lab_str(char* key, char* value) {
    printf("LAB %s=%s\n", key, value);
}
