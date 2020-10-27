/* These docx_*() functions generate docx content. Caller must call things in a
sensible order to create valid content - e.g. don't call docx_paragraph_start()
twice without intervening call to docx_paragraph_finish(). */

#include "../include/extract.h"

#include "docx_template.h"

#include "alloc.h"
#include "astring.h"
#include "document.h"
#include "docx.h"
#include "mem.h"
#include "memento.h"
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
    int     len;
    int     len2;
    char*   buffer;

    /* Find required length. */
    va_copy(va, va0);
    len = vsnprintf(NULL, 0, format, va);
    va_end(va);
    assert(len >= 0);
    len += 1; /* For terminating 0. */

    /* Repeat call of vnsprintf() with required buffer. */
    if (extract_malloc(&buffer, len)) return -1;
    va_copy(va, va0);
    len2 = vsnprintf(buffer, len, format, va);
    va_end(va);
    assert(len2 + 1 == len);
    *out = buffer;
    return len2;
}


static int local_asprintf(char** out, const char* format, ...)
{
    va_list va;
    int     ret;
    va_start(va, format);
    ret = local_vasprintf(out, format, va);
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
        double font_size,
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

int extract_docx_char_append_stringf(extract_astring_t* content, char* format, ...)
{
    char* buffer = NULL;
    int e;
    va_list va;
    va_start(va, format);
    e = extract_vasprintf(&buffer, format, va);
    va_end(va);
    if (e < 0) return e;
    e = extract_astring_cat(content, buffer);
    extract_free(&buffer);
    return e;
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

static int systemf(const char* format, ...)
/* Like system() but takes printf-style format and args. Also, if we return +ve
we set errno to EIO. */
{
    int e;
    char* command;
    va_list va;
    va_start(va, format);
    e = local_vasprintf(&command, format, va);
    va_end(va);
    if (e < 0) return e;
    outf("running: %s", command);
    e = system(command);
    extract_free(&command);
    if (e > 0) {
        errno = EIO;
    }
    return e;
}

/* Reads bytes until EOF and returns zero-terminated string in memory allocated
with realloc(). If error, we return NULL with errno set. */
static char* read_all(FILE* in)
{
    char*   ret = NULL;
    size_t  len = 0;
    size_t  delta = 128;
    for(;;) {
        size_t n;
        if (extract_realloc2(&ret, len, len + delta + 1)) {
            extract_free(&ret);
            return NULL;
        }
        n = fread(ret + len, 1 /*size*/, delta /*nmemb*/, in);
        len += n;
        if (feof(in)) {
            ret[len] = 0;
            return ret;
        }
        if (ferror(in)) {
            /* It's weird that fread() and ferror() don't set errno. */
            errno = EIO;
            extract_free(&ret);
            return NULL;
        }
    }
}

static int read_all_path(const char* path, char** o_text)
{
    int e = -1;
    FILE* f = NULL;
    char*   text = NULL;
    f = fopen(path, "rb");
    if (!f) goto end;
    text = read_all(f);
    if (!text) goto end;
    e = 0;
    end:
    if (f) fclose(f);
    if (e) extract_free(&text);
    else *o_text = text;
    return e;
}

static int write_all(const void* data, size_t data_size, const char* path)
{
    int e = -1;
    FILE* f = fopen(path, "w");
    if (!f) goto end;
    if (fwrite(data, data_size, 1 /*nmemb*/, f) != 1) goto end;
    e = 0;
    end:
    if (f) fclose(f);
    return e;
}

static int extract_docx_content_insert(
        const char* original,
        const char* mid_begin_name,
        const char* mid_end_name,
        const char* content,
        size_t      content_length,
        char**      o_out
        )
{
    int e = -1;
    const char* mid_begin;
    const char* mid_end;
    extract_astring_t   out;
    extract_astring_init(&out);
    
    mid_begin = strstr(original, mid_begin_name);
    if (!mid_begin) {
        outf("error: could not find '%s' in docx content",
                mid_begin_name);
        errno = ESRCH;
        goto end;
    }
    mid_end = strstr(mid_begin, mid_end_name);
    mid_begin += strlen(mid_begin_name);
    
    if (!mid_end) {
        outf("error: could not find '%s' in docx content",
                mid_end_name);
        errno = ESRCH;
        goto end;
    }

    if (extract_astring_catl(&out, original, mid_begin - original)) goto end;
    if (extract_astring_catl(&out, content, content_length)) goto end;
    if (extract_astring_cat(&out, mid_end)) goto end;
    
    *o_out = out.chars;
    out.chars = NULL;
    e = 0;
    
    end:
    if (e) {
        extract_astring_free(&out);
        *o_out = NULL;
    }
    return e;
}

static int s_find_mid(const char* text, const char* begin, const char* end, const char** o_begin, const char** o_end)
{
    *o_begin = strstr(text, begin);
    if (!*o_begin) goto fail;
    *o_begin += strlen(begin);
    *o_end = strstr(*o_begin, end);
    if (!*o_end) goto fail;
    return 0;
    fail:
    errno = ESRCH;
    return -1;
}


static int extract_docx_content_item(
        const char*                     content,
        size_t                          content_length,
        extract_document_imagesinfo_t*  imageinfos,
        const char*                     name,
        const char*                     text,
        char**                          text2
        )
/* Determines content of <name> in .docx file.

content
content_length
    Text to insert into template word/document.xml.
imageinfos
    Information about images.
name
    Path with .docx zip file.
text
    Content of <name> in template .docx.
text2
    Out-param. Set to NULL if <text> should be used unchanged. Otherwise set to
    point to desired text, allocated with malloc() which caller should free.
*/
{
    int e = -1;
    extract_astring_t   temp;
    extract_astring_init(&temp);
    *text2 = NULL;
    if (0) {}
    else if (!strcmp(name, "[Content_Types].xml")) {
        const char* begin;
        const char* end;
        const char* insert;
        extract_astring_free(&temp);
        outf("text: %s", text);
        if (s_find_mid(text, "<Types ", "</Types>", &begin, &end)) goto end;

        insert = begin;
        insert = strchr(insert, '>');
        assert(insert);
        insert += 1;

        if (extract_astring_catl(&temp, text, insert - text)) goto end;
        for (int it=0; it<imageinfos->imagetypes_num; ++it) {
            const char* imagetype = imageinfos->imagetypes[it];
            if (extract_astring_cat(&temp, "<Default Extension=\"")) goto end;
            if (extract_astring_cat(&temp, imagetype)) goto end;
            if (extract_astring_cat(&temp, "\" ContentType=\"image/")) goto end;
            if (extract_astring_cat(&temp, imagetype)) goto end;
            if (extract_astring_cat(&temp, "\"/>")) goto end;
        }
        if (extract_astring_cat(&temp, insert)) goto end;
        *text2 = temp.chars;
        extract_astring_init(&temp);
    }
    else if (!strcmp(name, "word/_rels/document.xml.rels")) {
        const char* begin;
        const char* end;
        int         j;
        extract_astring_free(&temp);
        if (s_find_mid(text, "<Relationships", "</Relationships>", &begin, &end)) goto end;
        if (extract_astring_catl(&temp, text, end - text)) goto end;
        outf("imageinfos.images_num=%i", imageinfos->images_num);
        for (j=0; j<imageinfos->images_num; ++j) {
            extract_document_image_t* image = &imageinfos->images[j];
            if (extract_astring_cat(&temp, "<Relationship Id=\"")) goto end;
            if (extract_astring_cat(&temp, image->id)) goto end;
            if (extract_astring_cat(&temp, "\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/image\" Target=\"media/")) goto end;
            if (extract_astring_cat(&temp, image->name)) goto end;
            if (extract_astring_cat(&temp, "\"/>")) goto end;
        }
        if (extract_astring_cat(&temp, end)) goto end;
        *text2 = temp.chars;
        extract_astring_init(&temp);
    }
    else if (!strcmp(name, "word/document.xml")) {
        if (extract_docx_content_insert(
                text,
                "<w:body>",
                "</w:body>",
                content,
                content_length,
                text2
                )) goto end;
    }
    else {
        *text2 = NULL;
    }
    e = 0;
    end:
    if (e) {
        extract_free(&*text2);
        extract_astring_free(&temp);
    }
    extract_astring_init(&temp);
    return e;
}
        
int extract_docx_content_to_docx(
        const char*         content,
        size_t              content_length,
        extract_document_t* document,
        extract_buffer_t*   buffer
        )
{
    int                             e = -1;
    extract_zip_t*                  zip = NULL;
    char*                           text2 = NULL;
    extract_document_imagesinfo_t   imageinfos = {0};
    int                             i;
    
    if (extract_zip_open(buffer, &zip)) goto end;
    if (extract_document_imageinfos(document, &imageinfos)) goto end;
    
    outf("imageinfos.images_num=%i imageinfos.imagetypes_num=%i",
            imageinfos.images_num, imageinfos.imagetypes_num);
    
    for (i=0; i<docx_template_items_num; ++i) {
        const docx_template_item_t* item = &docx_template_items[i];
        extract_free(&text2);
        outf("i=%i item->name=%s", i, item->name);
        if (extract_docx_content_item(
                content,
                content_length,
                &imageinfos,
                item->name,
                item->text,
                &text2
                )) goto end;
        {
            const char* text3 = (text2) ? text2 : item->text;
            if (extract_zip_write_file(zip, text3, strlen(text3), item->name)) goto end;
        }
    }
    
    for (i=0; i<imageinfos.images_num; ++i) {
        extract_document_image_t* image = &imageinfos.images[i];
        extract_free(&text2);
        if (extract_asprintf(&text2, "word/media/%s", image->name) < 0) goto end;
        if (extract_zip_write_file(zip, image->data, image->data_size, text2)) goto end;
    }
    
    if (extract_zip_close(zip)) goto end;
    zip = NULL;
    
    e = 0;
    
    end:
    if (e) outf("failed: %s", strerror(errno));
    extract_document_imageinfos_free(&imageinfos);
    extract_free(&text2);
    if (zip)    extract_zip_close(zip);
    
    return e;
}

static int check_path_shell_safe(const char* path)
/* Returns -1 with errno=EINVAL if <path> contains sequences that could make it
unsafe in shell commands. */
{
    if (0
            || strstr(path, "..")
            || strchr(path, '\'')
            || strchr(path, '"')
            || strchr(path, ' ')
            ) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}

static int remove_directory(const char* path)
{
    if (check_path_shell_safe(path)) {
        outf("path_out is unsafe: %s", path);
        return -1;
    }
    return systemf("rm -r '%s'", path);
}

#ifdef _WIN32
#include <direct.h>
static int s_mkdir(const char* path, int mode)
{
    (void) mode;
    return _mkdir(path);
}
#else
static int s_mkdir(const char* path, int mode)
{
    return mkdir(path, mode);
}
#endif


int extract_docx_content_to_docx_template(
        const char*         content,
        size_t              content_length,
        extract_document_t* document,
        const char*         path_template,
        const char*         path_out,
        int                 preserve_dir
        )
{
    int                             e = -1;
    int                             i;
    char*                           path_tempdir = NULL;
    FILE*                           f = NULL;
    char*                           path = NULL;
    char*                           text = NULL;
    char*                           text2 = NULL;
    extract_document_imagesinfo_t   imageinfos = {0};

    assert(path_out);
    assert(path_template);
    assert(content_length == strlen(content));
    
    if (check_path_shell_safe(path_out)) {
        outf("path_out is unsafe: %s", path_out);
        goto end;
    }

    if (extract_document_imageinfos(document, &imageinfos)) goto end;
    if (local_asprintf(&path_tempdir, "%s.dir", path_out) < 0) goto end;
    if (systemf("rm -r '%s' 2>/dev/null", path_tempdir) < 0) goto end;

    if (s_mkdir(path_tempdir, 0777)) {
        outf("Failed to create directory: %s", path_tempdir);
        goto end;
    }

    outf("Unzipping template document '%s' to tempdir: %s",
            path_template, path_tempdir);
    e = systemf("unzip -q -d '%s' '%s'", path_tempdir, path_template);
    if (e) {
        outf("Failed to unzip %s into %s",
                path_template, path_tempdir);
        goto end;
    }

    /* Might be nice to iterate through all items in path_tempdir, but for now
    we look at just the items that we know extract_docx_content_item() will
    modify. */
    
    {
        char*   names[] = {
                "word/document.xml",
                "[Content_Types].xml",
                "word/_rels/document.xml.rels",
                };
        int names_num = sizeof(names) / sizeof(names[0]);
        for (i=0; i<names_num; ++i) {
            const char* name = names[i];
            extract_free(&path);
            extract_free(&text);
            extract_free(&text2);
            if (extract_asprintf(&path, "%s/%s", path_tempdir, name) < 0) goto end;
            if (read_all_path(path, &text)) goto end;
            if (extract_docx_content_item(
                    content,
                    content_length,
                    &imageinfos,
                    name,
                    text,
                    &text2
                    )) goto end;
            {
                const char* text3 = (text2) ? text2 : text;
                if (write_all(text3, strlen(text3), path)) goto end;
            }
        }
    }

    /* Copy images into <path_tempdir>/media/. */
    extract_free(&path);
    if (extract_asprintf(&path, "%s/word/media", path_tempdir) < 0) goto end;
    if (s_mkdir(path, 0777)) goto end;
    
    for (i=0; i<imageinfos.images_num; ++i) {
        extract_document_image_t* image = &imageinfos.images[i];
        extract_free(&path);
        if (extract_asprintf(&path, "%s/word/media/%s", path_tempdir, image->name) < 0) goto end;
        if (write_all(image->data, image->data_size, path)) goto end;
    }
    
    outf("Zipping tempdir to create %s", path_out);
    {
        const char* path_out_leaf = strrchr(path_out, '/');
        if (!path_out_leaf) path_out_leaf = path_out;
        e = systemf("cd '%s' && zip -q -r -D '../%s' .", path_tempdir, path_out_leaf);
        if (e) {
            outf("Zip command failed to convert '%s' directory into output file: %s",
                    path_tempdir, path_out);
            goto end;
        }
    }

    if (!preserve_dir) {
        if (remove_directory(path_tempdir)) goto end;
    }

    e = 0;

    end:
    outf("e=%i", e);
    extract_document_imageinfos_free(&imageinfos);
    extract_free(&path_tempdir);
    extract_free(&path);
    extract_free(&text);
    extract_free(&text2);
    if (f)  fclose(f);

    if (e) {
        outf("Failed to create %s", path_out);
    }
    return e;
}
