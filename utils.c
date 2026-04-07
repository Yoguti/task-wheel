#include "utils.h"

void *safe_realloc(void *__ptr, size_t __size) {
    void *ptr = realloc(__ptr, __size);
    if (ptr == NULL) {
        fprintf(stderr, "Failed to reallocate memory\n");
        exit(EXIT_FAILURE);
    }
    return ptr;
}
void *safe_malloc(size_t __size) {
    void *ptr = malloc(__size);
    if (ptr == NULL) {
        fprintf(stderr, "Failed to allocate memory\n");
        exit(EXIT_FAILURE);
    }
    return ptr;
}