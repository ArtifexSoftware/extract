#pragma once

#include <stdlib.h>

void* extract_malloc(size_t size);

void* extract_realloc(void* ptr, size_t oldsize, size_t newsize);
/* Like realloc() but requires the existing buffer size, so that we can
internally overallocate to avoid too many calls to realloc(). If <ptr>
is NULL, <oldsize> is ignored. */

void extract_free(void* ptr);

typedef struct
{
    int num_malloc;
    int num_realloc;
    int num_free;
    int num_libc_realloc;
} extract_alloc_info_t;

extern extract_alloc_info_t extract_alloc_info;

void extract_alloc_exp_min(size_t size);
/* If size is non-zero, sets minimum actual allocation size, and we only
allocate in powers of two times this size. This is an attempt to improve speed
with memento squeeze. Default is 0 (every call to extract_realloc() calls
realloc(). */
