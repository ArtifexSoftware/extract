#include "../include/extract_alloc.h"

#include "astring.h"
#include "memento.h"

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

