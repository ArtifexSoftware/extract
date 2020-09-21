#ifndef ARTIFEX_OUTF_H
#define ARTIFEX_OUTF_H

/* Only for internal use by extract code.  */

void (outf)(
        int level,
        const char* file, int line,
        const char* fn,
        int ln,
        const char* format,
        ...
        );
/* Outputs text if <level> is less than level set by outf_level_set(). */

#define outf(format, ...) \
        (outf)(1, __FILE__, __LINE__, __FUNCTION__, 1 /*ln*/, format, ##__VA_ARGS__)

#define outf0(format, ...) \
        (outf)(0, __FILE__, __LINE__, __FUNCTION__, 1 /*ln*/, format, ##__VA_ARGS__)

#define outfx(format, ...)

/* Simple printf-style debug output. */

#define outfx(format, ...)

void outf_level_set(int level);

#endif
