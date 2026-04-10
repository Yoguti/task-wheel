#ifndef UTILS_H
#define UTILS_H

#include <stdlib.h>
#include <stdio.h>

void *safe_realloc(void *__ptr, size_t __size);
void *safe_malloc(size_t __size);

#endif