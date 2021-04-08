#include "../include/extract_alloc.h"

#include "astring.h"
#include "mem.h"
#include "memento.h"

#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>


void extract_astring_init(extract_astring_t* string)
{
    string->chars = NULL;
    string->chars_num = 0;
}

void extract_astring_free(extract_alloc_t* alloc, extract_astring_t* string)
{
    extract_free(alloc, &string->chars);
    extract_astring_init(string);
}


int extract_astring_catl(extract_alloc_t* alloc, extract_astring_t* string, const char* s, size_t s_len)
{
    if (extract_realloc2(alloc, &string->chars, string->chars_num+1, string->chars_num + s_len + 1)) return -1;
    memcpy(string->chars + string->chars_num, s, s_len);
    string->chars[string->chars_num + s_len] = 0;
    string->chars_num += s_len;
    return 0;
}

int extract_astring_catc(extract_alloc_t* alloc, extract_astring_t* string, char c)
{
    return extract_astring_catl(alloc, string, &c, 1);
}

int extract_astring_cat(extract_alloc_t* alloc, extract_astring_t* string, const char* s)
{
    return extract_astring_catl(alloc, string, s, strlen(s));
}

int extract_astring_catf(extract_alloc_t* alloc, extract_astring_t* string, const char* format, ...)
{
    char* buffer = NULL;
    int e;
    va_list va;
    va_start(va, format);
    e = extract_vasprintf(alloc, &buffer, format, va);
    va_end(va);
    if (e < 0) return e;
    e = extract_astring_cat(alloc, string, buffer);
    extract_free(alloc, &buffer);
    return e;
}

int extract_astring_truncate(extract_astring_t* content, int len)
{
    assert((size_t) len <= content->chars_num);
    content->chars_num -= len;
    content->chars[content->chars_num] = 0;
    return 0;
}

int astring_char_truncate_if(extract_astring_t* content, char c)
{
    if (content->chars_num && content->chars[content->chars_num-1] == c) {
        extract_astring_truncate(content, 1);
    }
    return 0;
}

