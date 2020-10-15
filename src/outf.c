#include "outf.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static int s_verbose = 0;

void outf_verbose_set(int verbose)
{
    s_verbose = verbose;
}

void (outf)(
        int level,
        const char* file, int line,
        const char* fn,
        int ln,
        const char* format,
        ...
        )
{
    if (level > s_verbose) {
        return;
    }
    va_list va;
    
    if (ln) {
        fprintf(stderr, "%s:%i:%s: ", file, line, fn);
    }
    va_start(va, format);
    vfprintf(stderr, format, va);
    va_end(va);
    if (ln) {
        size_t len = strlen(format);
        if (len == 0 || format[len-1] != '\n') {
            fprintf(stderr, "\n");
        }
    }
}
