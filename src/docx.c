/* These docx_*() functions generate docx content. Caller must call things in a
sensible order to create valid content - e.g. don't call docx_paragraph_start()
twice without intervening call to docx_paragraph_finish(). */

#include "../include/extract.h"

#include "docx_template.h"

#include "astring.h"
#include "docx.h"
#include "outf.h"
#include "zip.h"

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/stat.h>


static int local_vasprintf(char** out, const char* format, va_list va0)
{
    va_list va;

    /* Find required length. */
    va_copy(va, va0);
    int len = vsnprintf(NULL, 0, format, va);
    va_end(va);
    assert(len >= 0);
    len += 1; /* For terminating 0. */

    /* Repeat call of vnsprintf() with required buffer. */
    char* buffer = malloc(len);
    if (!buffer) {
        return -1;
    }
    va_copy(va, va0);
    int len2 = vsnprintf(buffer, len, format, va);
    va_end(va);
    assert(len2 + 1 == len);
    *out = buffer;
    return len2;
}


static int local_asprintf(char** out, const char* format, ...)
{
    va_list va;
    va_start(va, format);
    int ret = local_vasprintf(out, format, va);
    va_end(va);
    return ret;
}


int extract_docx_paragraph_start(extract_astring_t* content)
{
    return extract_astring_cat(content, "\n\n<w:p>");
}

int extract_docx_paragraph_finish(extract_astring_t* content)
{
    return extract_astring_cat(content, "\n</w:p>");
}

/* Starts a new run. Caller must ensure that extract_docx_run_finish() was called to
terminate any previous run. */
int extract_docx_run_start(
        extract_astring_t* content,
        const char* font_name,
        float font_size,
        int bold,
        int italic
        )
{
    int e = 0;
    if (!e) e = extract_astring_cat(content, "\n<w:r><w:rPr><w:rFonts w:ascii=\"");
    if (!e) e = extract_astring_cat(content, font_name);
    if (!e) e = extract_astring_cat(content, "\" w:hAnsi=\"");
    if (!e) e = extract_astring_cat(content, font_name);
    if (!e) e = extract_astring_cat(content, "\"/>");
    if (!e && bold) e = extract_astring_cat(content, "<w:b/>");
    if (!e && italic) e = extract_astring_cat(content, "<w:i/>");
    {
        char   font_size_text[32];
        if (0) font_size = 10;

        if (!e) e = extract_astring_cat(content, "<w:sz w:val=\"");
        snprintf(font_size_text, sizeof(font_size_text), "%f", font_size * 2);
        extract_astring_cat(content, font_size_text);
        extract_astring_cat(content, "\"/>");

        if (!e) e = extract_astring_cat(content, "<w:szCs w:val=\"");
        snprintf(font_size_text, sizeof(font_size_text), "%f", font_size * 1.5);
        extract_astring_cat(content, font_size_text);
        extract_astring_cat(content, "\"/>");
    }
    if (!e) e = extract_astring_cat(content, "</w:rPr><w:t xml:space=\"preserve\">");
    assert(!e);
    return e;

}
int extract_docx_run_finish(extract_astring_t* content)
{
    return extract_astring_cat(content, "</w:t></w:r>");
}

int extract_docx_char_append_string(extract_astring_t* content, char* text)
{
    return extract_astring_cat(content, text);
}

int extract_docx_char_append_char(extract_astring_t* content, char c)
{
    return extract_astring_catc(content, c);
}

int extract_docx_paragraph_empty(extract_astring_t* content)
{
    int e = -1;
    if (extract_docx_paragraph_start(content)) goto end;
    /* It seems like our choice of font size here doesn't make any difference
    to the ammount of vertical space, unless we include a non-space
    character. Presumably something to do with the styles in the template
    document. */
    if (extract_docx_run_start(
            content,
            "OpenSans",
            10 /*font_size*/,
            0 /*font_bold*/,
            0 /*font_italic*/
            )) goto end;
    //docx_char_append_string(content, "&#160;");   /* &#160; is non-break space. */
    if (extract_docx_run_finish(content)) goto end;
    if (extract_docx_paragraph_finish(content)) goto end;
    e = 0;
    end:
    return e;
}

/* Removes last <len> chars. */
static int docx_char_truncate(extract_astring_t* content, int len)
{
    assert(len <= content->chars_num);
    content->chars_num -= len;
    content->chars[content->chars_num] = 0;
    return 0;
}

int extract_docx_char_truncate_if(extract_astring_t* content, char c)
{
    if (content->chars_num && content->chars[content->chars_num-1] == c) {
        docx_char_truncate(content, 1);
    }
    return 0;
}

/* Like system() but takes printf-style format and args. */
static int systemf(const char* format, ...)
{
    char* command;
    va_list va;
    va_start(va, format);
    int e = local_vasprintf(&command, format, va);
    va_end(va);
    if (e < 0) return e;
    outf("running: %s", command);
    e = system(command);
    free(command);
    return e;
}

/* Reads bytes until EOF and returns zero-terminated string in memory allocated
with realloc(). If error, we return NULL with errno set. */
static char* read_all(FILE* in)
{
    char*   ret = NULL;
    int     len = 0;
    size_t  delta = 128;
    for(;;) {
        char* p = realloc(ret, len + delta + 1);
        if (!p) {
            free(ret);
            return NULL;
        }
        ret = p;
        ssize_t n = fread(ret + len, 1 /*size*/, delta /*nmemb*/, in);
        len += n;
        if (feof(in)) {
            ret[len] = 0;
            return ret;
        }
        if (ferror(in)) {
            /* It's weird that fread() and ferror() don't set errno. */
            errno = EIO;
            free(ret);
            return NULL;
        }
    }
}



static int extract_docx_content_replace(
        const char* original,
        const char* content,
        int         content_length,
        char**      o_out
        )
/*
original:
    Contents of word/document.xml from template.
content:
    Text to insert into <original>
o_out:
    Out-param, points to zero-terminated text allocated with malloc().
*/
{
    int e = -1;
    const char* original_marker = "<w:body>";
    const char* original_pos = strstr(original, original_marker);
    if (!original_pos) {
        outf("error: could not find '%s' in docx content",
                original_marker);
        errno = ESRCH;
        goto end;
    }
    original_pos += strlen(original_marker);

    extract_astring_t   out;
    extract_astring_init(&out);
    if (extract_astring_catl(&out, original, original_pos - original)) goto end;
    if (extract_astring_catl(&out, content, content_length)) goto end;
    if (extract_astring_cat(&out, original_pos)) goto end;
    
    *o_out = out.chars;
    out.chars = NULL;
    e = 0;
    
    end:
    return e;
}

int extract_docx_content_to_docx(
        const char*         content,
        size_t              content_length,
        extract_buffer_t*   buffer
        )
{
    int e = -1;
    char* content2;
    if (extract_docx_content_replace(
            extract_docx_word_document_xml,
            content,
            content_length,
            &content2
            )) goto end;

    extract_zip_t*  zip;
    if (extract_zip_open(buffer, &zip)) goto end;
    if (extract_docx_write(zip, content2, strlen(content2))) goto end;
    if (extract_zip_close(zip)) goto end;
    e = 0;
    
    end:
    return e;
}

int extract_docx_content_to_docx_template(
        const char* content,
        size_t      content_length,
        const char* path_template,
        const char* path_out,
        int         preserve_dir
        )
{
    assert(path_out);
    assert(path_template);
    assert(content_length == strlen(content));
    
    /* This gets set to zero only if everything succeeds. */
    int ret = -1;

    char*   path_tempdir = NULL;
    char*   word_document_xml = NULL;
    char*   original = NULL;
    FILE*   f = NULL;

    int e;

    if (0
            || strstr(path_out, "..")
            || strchr(path_out, '\'')
            || strchr(path_out, '"')
            || strchr(path_out, ' ')
            ) {
        outf("path_out is unsafe: %s", path_out);
        errno = EINVAL;
        goto end;
    }

    if (local_asprintf(&path_tempdir, "%s.dir", path_out) < 0) goto end;
    if (systemf("rm -r '%s' 2>/dev/null", path_tempdir) < 0) goto end;

    if (mkdir(path_tempdir, 0777)) {
        outf("Failed to create directory: %s", path_tempdir);
        goto end;
    }

    outfx("Unzipping template document '%s' to tempdir: %s",
            path_template, path_tempdir);
    e = systemf("unzip -q -d %s %s", path_tempdir, path_template);
    if (e) {
        outf("Failed to unzip %s into %s",
                path_template, path_tempdir);
        if (e > 0) errno = EIO;
        goto end;
    }

    if (local_asprintf(
            &word_document_xml,
            "%s/word/document.xml",
            path_tempdir) < 0) goto end;

    outfx("Reading tempdir's word/document.xml object");
    f = fopen(word_document_xml, "r");
    if (!f) {
        outf("Failed to open docx object: %s", word_document_xml);
        goto end;
    }
    original = read_all(f);
    if (!original) goto end;
    if (fclose(f)) goto end;
    f = NULL;

    char*   content2;
    if (extract_docx_content_replace(
            original,
            content,
            content_length,
            &content2
            )) goto end;
    f = fopen(word_document_xml, "w");
    if (fwrite(content2, strlen(content2), 1 /*nmemb*/, f) != 1) goto end;
    if (fclose(f) < 0) goto end;
    f = NULL;

    outf("Zipping tempdir to create %s", path_out);
    const char* path_out_leaf = strrchr(path_out, '/');
    if (!path_out_leaf) path_out_leaf = path_out;
    e = systemf("cd %s && zip -q -r ../%s .", path_tempdir, path_out_leaf);
    if (e) {
        outf("Zip command failed to convert '%s' directory into output file: %s",
                path_tempdir, path_out);
        if (e > 0) errno = EIO;
        goto end;
    }

    if (!preserve_dir) {
        if (strchr(path_tempdir, '\'') || strstr(path_tempdir, "..")) {
            outf("Refusing to delete path_tempdir=%s because not safe for shell");
        }
        e = systemf("rm -r '%s'", path_tempdir);
        if (e) {
            outf("error: Failed to delete tempdir: %s", path_tempdir);
            if (e > 0) errno = EIO;
            goto end;
        }
    }

    ret = 0;

    end:
    if (path_tempdir)   free(path_tempdir);
    if (word_document_xml)  free(word_document_xml);
    if (original)   free(original);
    if (f)  fclose(f);

    return ret;
}
