#include "astring.h"

#include <stdlib.h>
#include <string.h>


void extract_astring_init(extract_astring_t* string)
{
    string->chars = NULL;
    string->chars_num = 0;
}

void extract_astring_free(extract_astring_t* string)
{
    free(string->chars);
    extract_astring_init(string);
}

int extract_astring_catl(extract_astring_t* string, const char* s, int s_len)
{
    char* chars = realloc(string->chars, string->chars_num + s_len + 1);
    if (!chars) return -1;
    memcpy(chars + string->chars_num, s, s_len);
    chars[string->chars_num + s_len] = 0;
    string->chars = chars;
    string->chars_num += s_len;
    return 0;
}

int extract_astring_catc(extract_astring_t* string, char c)
{
    return extract_astring_catl(string, &c, 1);
}

int extract_astring_cat(extract_astring_t* string, const char* s)
{
    return extract_astring_catl(string, s, strlen(s));
}

