#include "alloc.h"
#include "memento.h"

#include <assert.h>
#include <stdlib.h>


extract_alloc_info_t    extract_alloc_info = {0};

static size_t s_exp_min_alloc_size = 0;

static int round_up(size_t n)
{
    if (s_exp_min_alloc_size) {
        /* Round up to power of two. */
        if (n==0) return 0;
        size_t ret = s_exp_min_alloc_size;
        for(;;) {
            if (ret >= n) return ret;
            size_t ret_old = ret;
            ret *= 2;
            assert(ret > ret_old);
        }
    }
    else {
        return n;
    }
}

void* extract_malloc(size_t size)
{
    size = round_up(size);
    extract_alloc_info.num_malloc += 1;
    void* ret = malloc(size);
    return ret;
}

void* extract_realloc(void* ptr, size_t oldsize, size_t newsize)
{
    /* We ignore <oldsize> if <ptr> is NULL - allows callers to not worry about
    edge cases e.g. with strlen+1. */
    oldsize = ptr ? round_up(oldsize) : 0;
    newsize = round_up(newsize);
    extract_alloc_info.num_realloc += 1;
    if (newsize == oldsize) return ptr;
    extract_alloc_info.num_libc_realloc += 1;
    return realloc(ptr, newsize);
}

void extract_free(void* ptr)
{
    extract_alloc_info.num_free += 1;
    free(ptr);
}

void extract_alloc_exp_min(size_t size)
{
    s_exp_min_alloc_size = size;
}
