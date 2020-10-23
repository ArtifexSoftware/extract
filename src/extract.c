#include "../include/extract.h"

#include "alloc.h"
#include "astring.h"
#include "document.h"
#include "docx.h"
#include "memento.h"
#include "mem.h"
#include "outf.h"
#include "xml.h"

#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static const float g_pi = 3.14159265f;


typedef struct
{
    float   a;
    float   b;
    float   c;
    float   d;
    float   e;
    float   f;
} matrix_t;

static float matrix_expansion(matrix_t m)
{
    return sqrtf(fabsf(m.a * m.d - m.b * m.c));
}

/* Unused but useful to keep code here. */
#if 0
static void matrix_scale(matrix_t* matrix, float scale)
{
    matrix->a *= scale;
    matrix->b *= scale;
    matrix->c *= scale;
    matrix->d *= scale;
    matrix->e *= scale;
    matrix->f *= scale;
}

static void matrix_scale4(matrix_t* matrix, float scale)
{
    matrix->a *= scale;
    matrix->b *= scale;
    matrix->c *= scale;
    matrix->d *= scale;
}
#endif

static const char* matrix_string(const matrix_t* matrix)
{
    static char ret[64];
    snprintf(ret, sizeof(ret), "{%f %f %f %f %f %f}",
            matrix->a,
            matrix->b,
            matrix->c,
            matrix->d,
            matrix->e,
            matrix->f
            );
    return ret;
}

typedef struct
{
    float x;
    float y;
} point_t;


/* A single char in a span.
*/
typedef struct
{
    /* (x,y) before transformation by ctm and trm. */
    float       pre_x;
    float       pre_y;
    
    /* (x,y) after transformation by ctm and trm. */
    float       x;
    float       y;
    
    int         gid;
    unsigned    ucs;
    float       adv;
} char_t;

static void char_init(char_t* item)
{
    item->pre_x = 0;
    item->pre_y = 0;
    item->x = 0;
    item->y = 0;
    item->gid = 0;
    item->ucs = 0;
    item->adv = 0;
}


/* Array of chars that have same font and are usually adjacent.
*/
typedef struct
{
    matrix_t    ctm;
    matrix_t    trm;
    char*       font_name;
    
    /* font size is matrix_expansion(trm). */
    
    struct {
        int font_bold   : 1;
        int font_italic : 1;
        int wmode       : 1;
    };
    
    char_t*     chars;
    int         chars_num;
} span_t;

/* Returns static string containing info about span_t. */
static const char* span_string(span_t* span)
{
    static extract_astring_t ret = {0};
    float x0 = 0;
    float y0 = 0;
    float x1 = 0;
    float y1 = 0;
    int c0 = 0;
    int c1 = 0;
    int i;
    extract_astring_free(&ret);
    if (!span) return NULL;
    if (span->chars_num) {
        c0 = span->chars[0].ucs;
        x0 = span->chars[0].x;
        y0 = span->chars[0].y;
        c1 = span->chars[span->chars_num-1].ucs;
        x1 = span->chars[span->chars_num-1].x;
        y1 = span->chars[span->chars_num-1].y;
    }
    {
        char buffer[200];
        snprintf(buffer, sizeof(buffer),
                "span chars_num=%i (%c:%f,%f)..(%c:%f,%f) font=%s:(%f,%f) wmode=%i chars_num=%i: ",
                span->chars_num,
                c0, x0, y0,
                c1, x1, y1,
                span->font_name,
                span->trm.a,
                span->trm.d,
                span->wmode,
                span->chars_num
                );
        extract_astring_cat(&ret, buffer);
        for (i=0; i<span->chars_num; ++i) {
            snprintf(
                    buffer,
                    sizeof(buffer),
                    " i=%i {x=%f adv=%f}",
                    i,
                    span->chars[i].x,
                    span->chars[i].adv
                    );
            extract_astring_cat(&ret, buffer);
        }
    }
    extract_astring_cat(&ret, ": ");
    extract_astring_catc(&ret, '"');
    for (i=0; i<span->chars_num; ++i) {
        extract_astring_catc(&ret, (char) span->chars[i].ucs);
    }
    extract_astring_catc(&ret, '"');
    return ret.chars;
}

/* Returns static string containing brief info about span_t. */
static const char* span_string2(span_t* span)
{
    static extract_astring_t ret = {0};
    int i;
    extract_astring_free(&ret);
    extract_astring_catc(&ret, '"');
    for (i=0; i<span->chars_num; ++i) {
        extract_astring_catc(&ret, (char) span->chars[i].ucs);
    }
    extract_astring_catc(&ret, '"');
    return ret.chars;
}

/* Appends new char_t to an span_t with .ucs=c and all other
fields zeroed. */
static int span_append_c(span_t* span, int c)
{
    char_t* item;
    if (extract_realloc2(
            &span->chars,
            sizeof(*span->chars) * span->chars_num,
            sizeof(*span->chars) * (span->chars_num + 1)
            )) {
        return -1;
    }
    item = &span->chars[span->chars_num];
    span->chars_num += 1;
    char_init(item);
    item->ucs = c;
    return 0;
}

static char_t* span_char_first(span_t* span)
{
    assert(span->chars_num > 0);
    return &span->chars[0];
}

static char_t* span_char_last(span_t* span)
{
    assert(span->chars_num > 0);
    return &span->chars[span->chars_num-1];
}

/*
static void span_extend_point(span_t* span, point_t* point)
{
    float x = span_char_last(span)->pre_x;
    x += span_char_last(span)->adv * matrix_expansion(span->trm);
    if (x > point->x) {
        point->x = x;
    }
    float y = span_char_first(span)->pre_y;
    if (y > point->y) {
        point->y = y;
    };
}
*/

static float span_angle(span_t* span)
{
    /* Assume ctm is a rotation matix. */
    float ret = atan2f(-span->ctm.c, span->ctm.a);
    outfx("ctm.a=%f ctm.b=%f ret=%f", span->ctm.a, span->ctm.b, ret);
    return ret;
    /* Not sure whether this is right. Inclined text seems to be done by
    setting the ctm matrix, so not really sure what trm matrix does. This code
    assumes that it also inclines text, but maybe it only rotates individual
    glyphs? */
    /*if (span->wmode == 0) {
        return atan2f(span->trm.b, span->trm.a);
    }
    else {
        return atan2f(span->trm.d, span->trm.c);
    }*/
}

/* Returns total width of span. */
static float span_adv_total(span_t* span)
{
    float dx = span_char_last(span)->x - span_char_first(span)->x;
    float dy = span_char_last(span)->y - span_char_first(span)->y;
    /* We add on the advance of the last item; this avoids us returning zero if
    there's only one item. */
    float adv = span_char_last(span)->adv * matrix_expansion(span->trm);
    return sqrtf(dx*dx + dy*dy) + adv;
}

/* Returns distance between end of <a> and beginning of <b>. */
static float spans_adv(
        span_t* a_span,
        char_t* a,
        char_t* b
        )
{
    float delta_x = b->x - a->x;
    float delta_y = b->y - a->y;
    float s = sqrtf( delta_x*delta_x + delta_y*delta_y);
    float a_size = a->adv * matrix_expansion(a_span->trm);
    s -= a_size;
    return s;
}

/* Array of pointers to spans that are aligned on same line.
*/
typedef struct
{
    span_t**    spans;
    int         spans_num;
} line_t;

/* Unused but useful to keep code here. */
#if 0
/* Returns static string containing info about line_t. */
static const char* line_string(line_t* line)
{
    static extract_astring_t ret = {0};
    char    buffer[32];
    extract_astring_free(&ret);
    snprintf(buffer, sizeof(buffer), "line spans_num=%i:", line->spans_num);
    extract_astring_cat(&ret, buffer);
    int i;
    for (i=0; i<line->spans_num; ++i) {
        extract_astring_cat(&ret, " ");
        extract_astring_cat(&ret, span_string(line->spans[i]));
    }
    return ret.chars;
}
#endif

/* Returns static string containing brief info about line_t. */
static const char* line_string2(line_t* line)
{
    static extract_astring_t ret = {0};
    char    buffer[256];
    int i;
    extract_astring_free(&ret);
    snprintf(buffer, sizeof(buffer), "line x=%f y=%f spans_num=%i:",
            line->spans[0]->chars[0].x,
            line->spans[0]->chars[0].y,
            line->spans_num
            );
    extract_astring_cat(&ret, buffer);
    for (i=0; i<line->spans_num; ++i) {
        extract_astring_cat(&ret, " ");
        extract_astring_cat(&ret, span_string2(line->spans[i]));
    }
    return ret.chars;
}

/* Returns first span in a line. */
static span_t* line_span_last(line_t* line)
{
    assert(line->spans_num > 0);
    return line->spans[line->spans_num - 1];
}

/* Returns list span in a line. */
static span_t* line_span_first(line_t* line)
{
    assert(line->spans_num > 0);
    return line->spans[0];
}

/* Returns first char_t in a line. */
static char_t* line_item_first(line_t* line)
{
    span_t* span = line_span_first(line);
    return span_char_first(span);
}

/* Returns last char_t in a line. */
static char_t* line_item_last(line_t* line)
{
    span_t* span = line_span_last(line);
    return span_char_last(span);
}

/* Returns angle of <line>. */
static float line_angle(line_t* line)
{
    /* All spans in a line must have same angle, so just use the first span. */
    assert(line->spans_num > 0);
    return span_angle(line->spans[0]);
}

/*
static void line_extend_point(line_t* line, point_t* point)
{
    span_extend_point(line_span_last(line), point);
}
*/

/* Array of pointers to lines that are aligned and adjacent to each other so as
to form a paragraph. */
typedef struct
{
    line_t**    lines;
    int         lines_num;
} paragraph_t;

static const char* paragraph_string(paragraph_t* paragraph)
{
    static extract_astring_t ret = {0};
    extract_astring_free(&ret);
    extract_astring_cat(&ret, "paragraph: ");
    if (paragraph->lines_num) {
        extract_astring_cat(&ret, line_string2(paragraph->lines[0]));
        if (paragraph->lines_num > 1) {
            extract_astring_cat(&ret, "..");
            extract_astring_cat(
                    &ret,
                    line_string2(paragraph->lines[paragraph->lines_num-1])
                    );
        }
    }
    return ret.chars;
}

/* Returns first line in paragraph. */
static line_t* paragraph_line_first(const paragraph_t* paragraph)
{
    assert(paragraph->lines_num);
    return paragraph->lines[0];
}

/* Returns last line in paragraph. */
static line_t* paragraph_line_last(const paragraph_t* paragraph)
{
    assert(paragraph->lines_num);
    return paragraph->lines[ paragraph->lines_num-1];
}

/*
static void paragraph_extend_point(const paragraph_t* paragraph, point_t* point)
{
    int i;
    for (i=0; i<paragraph->lines_num; ++i) {
        line_t* line = paragraph->lines[i];
        line_extend_point(line, point);
    }
}


*/

typedef struct
{
    char*   type;
    char*   name;
    char*   id;
    char*   data;
    size_t  data_size;
} image_t;

/* A page. Contains different representations of the same list of spans.
*/
typedef struct
{
    span_t**    spans;
    int         spans_num;

    /* .lines[] refers to items in .spans. */
    line_t**    lines;
    int         lines_num;

    /* .paragraphs[] refers to items in .lines. */
    paragraph_t**   paragraphs;
    int             paragraphs_num;
    
    image_t*        images;
    int             images_num;
} page_t;

static void page_init(page_t* page)
{
    page->spans = NULL;
    page->spans_num = 0;
    page->lines = NULL;
    page->lines_num = 0;
    page->paragraphs = NULL;
    page->paragraphs_num = 0;
    page->images = NULL;
    page->images_num = 0;
}

static void page_free(page_t* page)
{
    int s;
    if (!page) return;

    for (s=0; s<page->spans_num; ++s) {
        span_t* span = page->spans[s];
        if (span) {
            extract_free(&span->chars);
            extract_free(&span->font_name);
        }
        extract_free(&span);
    }
    extract_free(&page->spans);

    {
        int l;
        for (l=0; l<page->lines_num; ++l) {
            line_t* line = page->lines[l];
            extract_free(&line->spans);
            extract_free(&line);
            /* We don't free line->spans->chars[] because already freed via
            page->spans. */
        }
    }
    extract_free(&page->lines);

    {
        int p;
        for (p=0; p<page->paragraphs_num; ++p) {
            paragraph_t* paragraph = page->paragraphs[p];
            if (paragraph) extract_free(&paragraph->lines);
            extract_free(&paragraph);
        }
    }
    extract_free(&page->paragraphs);
    
    {
        int i;
        for (i=0; i<page->images_num; ++i) {
            extract_free(&page->images[i].data);
            extract_free(&page->images[i].type);
            extract_free(&page->images[i].id);
            extract_free(&page->images[i].name);
        }
    }
    extract_free(&page->images);
}

/* Appends new empty span_ to an page_t; returns NULL with errno set on error.
*/
static span_t* page_span_append(page_t* page)
{
    span_t* span;
    if (extract_malloc(&span, sizeof(*span))) return NULL;
    span->font_name = NULL;
    span->chars = NULL;
    span->chars_num = 0;
    if (extract_realloc2(
            &page->spans,
            sizeof(*page->spans) * page->spans_num,
            sizeof(*page->spans) * (page->spans_num + 1)
            )) {
        extract_free(&span);
        return NULL;
    }
    page->spans[page->spans_num] = span;
    page->spans_num += 1;
    return span;
}


/* Array of pointers to pages.
*/
struct extract_document_t
{
    page_t**    pages;
    int         pages_num;
};

int extract_document_imageinfos(extract_document_t* document, extract_document_imagesinfo_t* o_imageinfos)
{
    int e = -1;
    extract_document_imagesinfo_t   imageinfos = {0};
    
    for (int p=0; p<document->pages_num; ++p) {
        page_t* page = document->pages[p];
        for (int i=0; i<page->images_num; ++i) {
            image_t* image = &page->images[i];
            if (extract_realloc2(
                    &imageinfos.images,
                    sizeof(extract_document_image_t) * imageinfos.images_num,
                    sizeof(extract_document_image_t) * (imageinfos.images_num + 1)
                    )) goto end;
            outfx("image->name=%s image->id=%s", image->name, image->id);
            assert(image->name);
            imageinfos.images[imageinfos.images_num].name = image->name;
            imageinfos.images[imageinfos.images_num].id = image->id;
            imageinfos.images[imageinfos.images_num].data = image->data;
            imageinfos.images[imageinfos.images_num].data_size = image->data_size;
            imageinfos.images_num += 1;
            
            /* Add image type if we haven't seen it before. */
            {
                int it;
                for (it=0; it<imageinfos.imagetypes_num; ++it) {
                    if (!strcmp(imageinfos.imagetypes[it], image->type)) {
                        break;
                    }
                }
                if (it == imageinfos.imagetypes_num) {
                    if (extract_realloc2(
                            &imageinfos.imagetypes,
                            sizeof(char*) * imageinfos.imagetypes_num,
                            sizeof(char*) * (imageinfos.imagetypes_num + 1)
                            )) goto end;
                    assert(image->type);
                    imageinfos.imagetypes[imageinfos.imagetypes_num] = image->type;
                    imageinfos.imagetypes_num += 1;
                }
            }
        }
    }
    e = 0;
    end:
    if (e) {
    }
    else {
        *o_imageinfos = imageinfos;
    }
    return e;
}

void extract_document_imageinfos_free(extract_document_imagesinfo_t* imageinfos)
{
    extract_free(&imageinfos->images);
    extract_free(&imageinfos->imagetypes);
    imageinfos->images = NULL;
    imageinfos->imagetypes = NULL;
}

/* Appends new empty page_t to an extract_document_t; returns NULL with errno
set on error. */
static page_t* document_page_append(extract_document_t* document)
{
    page_t* page;
    if (extract_malloc(&page, sizeof(page_t))) return NULL;
    page->spans = NULL;
    page->spans_num = 0;
    page->lines = NULL;
    page->lines_num = 0;
    page->paragraphs = NULL;
    page->paragraphs_num = 0;
    if (extract_realloc2(
            &document->pages,
            sizeof(page_t*) * document->pages_num + 1,
            sizeof(page_t*) * (document->pages_num + 1)
            )) {
        extract_free(&page);
        return NULL;
    }
    page_init(page);
    document->pages[document->pages_num] = page;
    document->pages_num += 1;
    return page;
}

void extract_document_free(extract_document_t** io_document)
{
    extract_document_t* document = *io_document;
    int p;
    if (!document) {
        return;
    }
    for (p=0; p<document->pages_num; ++p) {
        page_t* page = document->pages[p];
        page_free(page);
        extract_free(&page);
    }
    extract_free(&document->pages);
    document->pages = NULL;
    document->pages_num = 0;
    extract_free(&document);
    *io_document = NULL;
}


/* Returns +1, 0 or -1 depending on sign of x. */
static int s_sign(float x)
{
    if (x < 0)  return -1;
    if (x > 0)  return +1;
    return 0;
}

/* Unused but useful to keep code here. */
#if 0
/* Returns zero if *lhs and *rhs are equal, otherwise +/- 1. */
static int matrix_cmp(
        const matrix_t* lhs,
        const matrix_t* rhs
        )
{
    int ret;
    ret = s_sign(lhs->a - rhs->a);  if (ret) return ret;
    ret = s_sign(lhs->b - rhs->b);  if (ret) return ret;
    ret = s_sign(lhs->c - rhs->c);  if (ret) return ret;
    ret = s_sign(lhs->d - rhs->d);  if (ret) return ret;
    ret = s_sign(lhs->e - rhs->e);  if (ret) return ret;
    ret = s_sign(lhs->f - rhs->f);  if (ret) return ret;
    return 0;
}
#endif

/* Returns zero if first four members of *lhs and *rhs are equal, otherwise
+/-1. */
static int matrix_cmp4(
        const matrix_t* lhs,
        const matrix_t* rhs
        )
{
    int ret;
    ret = s_sign(lhs->a - rhs->a);  if (ret) return ret;
    ret = s_sign(lhs->b - rhs->b);  if (ret) return ret;
    ret = s_sign(lhs->c - rhs->c);  if (ret) return ret;
    ret = s_sign(lhs->d - rhs->d);  if (ret) return ret;
    return 0;
}


static point_t multiply_matrix_point(matrix_t m, point_t p)
{
    float x = p.x;
    p.x = m.a * x + m.c * p.y;
    p.y = m.b * x + m.d * p.y;
    return p;
}

typedef struct
{
    point_t min;
    point_t max;
} rectangle_t;

static int s_matrix_read(const char* text, matrix_t* matrix)
{
    int n;
    if (!text) {
        outf("text is NULL in s_matrix_read()");
        errno = EINVAL;
        return -1;
    }
    n = sscanf(text,
            "%f %f %f %f %f %f",
            &matrix->a,
            &matrix->b,
            &matrix->c,
            &matrix->d,
            &matrix->e,
            &matrix->f
            );
    if (n != 6) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}

/* Unused but useful to keep code here. */
#if 0
/* Like s_matrix_read() but only expects four values, and sets .e and .g to
zero. */
static int s_matrix_read4(const char* text, matrix_t* matrix)
{
    if (!text) {
        outf("text is NULL in s_matrix_read4()");
        errno = EINVAL;
        return -1;
    }
    int n = sscanf(text,
            "%f %f %f %f",
            &matrix->a,
            &matrix->b,
            &matrix->c,
            &matrix->d
            );
    if (n != 4) {
        errno = EINVAL;
        return -1;
    }
    matrix->e = 0;
    matrix->f = 0;
    return 0;
}
#endif


/* Things for direct conversion of text spans into lines and paragraphs. */

/* Returns 1 if lines have same wmode and are at the same angle, else 0.

todo: allow small epsilon? */
static int lines_are_compatible(
        line_t* a,
        line_t* b,
        float           angle_a,
        int             verbose
        )
{
    if (a == b) return 0;
    if (!a->spans || !b->spans) return 0;
    if (line_span_first(a)->wmode != line_span_first(b)->wmode) {
        return 0;
    }
    if (matrix_cmp4(
            &line_span_first(a)->ctm,
            &line_span_first(b)->ctm
            )) {
        if (verbose) {
            outf("ctm's differ:");
            outf("    %f %f %f %f %f %f",
                    line_span_first(a)->ctm.a,
                    line_span_first(a)->ctm.b,
                    line_span_first(a)->ctm.c,
                    line_span_first(a)->ctm.d,
                    line_span_first(a)->ctm.e,
                    line_span_first(a)->ctm.f
                    );
            outf("    %f %f %f %f %f %f",
                    line_span_first(b)->ctm.a,
                    line_span_first(b)->ctm.b,
                    line_span_first(b)->ctm.c,
                    line_span_first(b)->ctm.d,
                    line_span_first(b)->ctm.e,
                    line_span_first(b)->ctm.f
                    );
        }
        return 0;
    }
    {
        float angle_b = span_angle(line_span_first(b));
        if (angle_b != angle_a) {
            outfx("%s:%i: angles differ");
            return 0;
        }
    }
    return 1;
}


/* Creates representation of span_t's that consists of a list of line_t's, with
each line_t contains pointers to a list of span_t's.

We only join spans that are at the same angle and are aligned.

On entry:
    Original value of *o_lines and *o_lines_num are ignored.

    <spans> points to array of <spans_num> span_t*'s, each pointing to
    an span_t.

On exit:
    If we succeed, we return 0, with *o_lines pointing to array of *o_lines_num
    line_t*'s, each pointing to an line_t.

    Otherwise we return -1 with errno set. *o_lines and *o_lines_num are
    undefined.
*/
static int make_lines(
        span_t**    spans,
        int         spans_num,
        line_t***   o_lines,
        int*        o_lines_num
        )
{
    int ret = -1;

    /* Make an line_t for each span. Then we will join some of these
    line_t's together before returning. */
    int         lines_num = spans_num;
    line_t**    lines = NULL;
    int         a;
    int         num_compatible;
    int         num_joins;
    if (extract_malloc(&lines, sizeof(*lines) * lines_num)) goto end;

    /* Ensure we can clean up after error. */
    for (a=0; a<lines_num; ++a) {
        lines[a] = NULL;
    }
    for (a=0; a<lines_num; ++a) {
        if (extract_malloc(&lines[a], sizeof(line_t))) goto end;
        lines[a]->spans_num = 0;
        if (extract_malloc(&lines[a]->spans, sizeof(span_t*) * 1)) goto end;
        lines[a]->spans_num = 1;
        lines[a]->spans[0] = spans[a];
        outfx("initial line a=%i: %s", a, line_string(lines[a]));
    }

    num_compatible = 0;

    /* For each line, look for nearest aligned line, and append if found. */
    num_joins = 0;
    for (a=0; a<lines_num; ++a) {
        int b;
        int verbose = 0;
        int nearest_line_b = -1;
        float nearest_adv = 0;
        line_t* nearest_line = NULL;
        span_t* span_a;
        float angle_a;
        
        line_t* line_a = lines[a];
        if (!line_a) {
            continue;
        }

        if (0 && a < 1) verbose = 1;
        outfx("looking at line_a=%s", line_string2(line_a));

        span_a = line_span_last(line_a);
        angle_a = span_angle(span_a);
        if (verbose) outf("a=%i angle_a=%lf ctm=%s: %s",
                a,
                angle_a * 180/g_pi,
                matrix_string(&span_a->ctm),
                line_string2(line_a)
                );

        for (b=0; b<lines_num; ++b) {
            line_t* line_b = lines[b];
            if (!line_b) {
                continue;
            }
            if (b == a) {
                continue;
            }
            if (verbose) {
                outf("");
                outf("a=%i b=%i: nearest_line_b=%i nearest_adv=%lf",
                        a,
                        b,
                        nearest_line_b,
                        nearest_adv
                        );
                outf("    line_a=%s", line_string2(line_a));
                outf("    line_b=%s", line_string2(line_b));
            }
            if (!lines_are_compatible(line_a, line_b, angle_a, 0*verbose)) {
                if (verbose) outf("not compatible");
                continue;
            }

            num_compatible += 1;
            {
                /* Find angle between last glyph of span_a and first glyph of
                span_b. This detects whether the lines are lined up with each other
                (as opposed to being at the same angle but in different lines). */
                span_t* span_b = line_span_first(line_b);
                float dx = span_char_first(span_b)->x - span_char_last(span_a)->x;
                float dy = span_char_first(span_b)->y - span_char_last(span_a)->y;
                float angle_a_b = atan2f(-dy, dx);
                const float angle_tolerance_deg = 1;
                if (verbose) {
                    outf("delta=(%f %f) alast=(%f %f) bfirst=(%f %f): angle_a=%lf angle_a_b=%lf",
                            dx,
                            dy,
                            span_char_last(span_a)->x,
                            span_char_last(span_a)->y,
                            span_char_first(span_b)->x,
                            span_char_first(span_b)->y,
                            angle_a * 180 / g_pi,
                            angle_a_b * 180 / g_pi
                            );
                }
                /* Might want to relax this when we test on non-horizontal lines.
                */
                if (fabs(angle_a_b - angle_a) * 180 / g_pi <= angle_tolerance_deg) {
                    /* Find distance between end of line_a and beginning of line_b. */
                    float adv = spans_adv(
                            span_a,
                            span_char_last(span_a),
                            span_char_first(span_b)
                            );
                    if (verbose) outf("nearest_adv=%lf. angle_a_b=%lf adv=%lf",
                            nearest_adv,
                            angle_a_b,
                            adv
                            );
                    if (!nearest_line || adv < nearest_adv) {
                        nearest_line = line_b;
                        nearest_adv = adv;
                        nearest_line_b = b;
                    }
                }
                else {
                    if (verbose) outf(
                            "angle beyond tolerance: span_a last=(%f,%f) span_b first=(%f,%f) angle_a_b=%lg angle_a=%lg span_a.trm{a=%f b=%f}",
                            span_char_last(span_a)->x,
                            span_char_last(span_a)->y,
                            span_char_first(span_b)->x,
                            span_char_first(span_b)->y,
                            angle_a_b * 180 / g_pi,
                            angle_a * 180 / g_pi,
                            span_a->trm.a,
                            span_a->trm.b
                            );
                }
            }
        }

        if (nearest_line) {
            /* line_a and nearest_line are aligned so we can move line_b's
            spans on to the end of line_a. */
            span_t* span_b = line_span_first(nearest_line);
            b = nearest_line_b;
            if (verbose) outf("found nearest line. a=%i b=%i", a, b);

            if (1
                    && span_char_last(span_a)->ucs != ' '
                    && span_char_first(span_b)->ucs != ' '
                    ) {
                /* Find average advance of the two adjacent spans in the two
                lines we are considering joining, so that we can decide whether
                the distance between them is large enough to merit joining with
                a space character). */
                float average_adv = (
                        (span_adv_total(span_a) + span_adv_total(span_b))
                        /
                        (float) (span_a->chars_num + span_b->chars_num)
                        );

                int insert_space = (nearest_adv > 0.25 * average_adv);
                if (insert_space) {
                    /* Append space to span_a before concatenation. */
                    char_t* item;
                    if (verbose) {
                        outf("(inserted space) nearest_adv=%lf average_adv=%lf",
                                nearest_adv,
                                average_adv
                                );
                        outf("    a: %s", span_string(span_a));
                        outf("    b: %s", span_string(span_b));
                    }
                    if (extract_realloc2(
                            &span_a->chars,
                            sizeof(char_t) * span_a->chars_num,
                            sizeof(char_t) * (span_a->chars_num + 1)
                            )) goto end;
                    item = &span_a->chars[span_a->chars_num];
                    span_a->chars_num += 1;
                    extract_bzero(item, sizeof(*item));
                    item->ucs = ' ';
                    item->adv = nearest_adv;
                }

                if (verbose) {
                    outf("Joining spans a=%i b=%i:", a, b);
                    outf("    %s", span_string2(span_a));
                    outf("    %s", span_string2(span_b));
                }
                if (0) {
                    /* Show details about what we're joining. */
                    outf(
                            "joining line insert_space=%i a=%i (y=%f) to line b=%i (y=%f). nearest_adv=%lf average_adv=%lf",
                            insert_space,
                            a,
                            span_char_last(span_a)->y,
                            b,
                            span_char_first(span_b)->y,
                            nearest_adv,
                            average_adv
                            );
                    outf("a: %s", span_string(span_a));
                    outf("b: %s", span_string(span_b));
                }
            }

            /* We might end up with two adjacent spaces here. But removing a
            space could result in an empty line_t, which could break various
            assumptions elsewhere. */

            if (verbose) {
                outf("Joining spans a=%i b=%i:", a, b);
                outf("    %s", span_string2(span_a));
                outf("    %s", span_string2(span_b));
            }
            if (extract_realloc2(
                    &line_a->spans,
                    sizeof(span_t*) * line_a->spans_num,
                    sizeof(span_t*) * (line_a->spans_num + nearest_line->spans_num)
                    )) goto end;
            {
                int k;
                for (k=0; k<nearest_line->spans_num; ++k) {
                    line_a->spans[ line_a->spans_num + k] = nearest_line->spans[k];
                }
            }
            line_a->spans_num += nearest_line->spans_num;

            /* Ensure that we ignore nearest_line from now on. */
            extract_free(&nearest_line->spans);
            extract_free(&nearest_line);
            outfx("setting line[b=%i] to NULL", b);
            lines[b] = NULL;

            num_joins += 1;

            if (b > a) {
                /* We haven't yet tried appending any spans to nearest_line, so
                the new extended line_a needs checking again. */
                a -= 1;
            }
            outfx("new line is:\n    %s", line_string2(line_a));
        }
    }

    {
        /* Remove empty lines left behind after we appended pairs of lines. */
        int from;
        int to;
        int lines_num_old;
        for (from=0, to=0; from<lines_num; ++from) {
            if (lines[from]) {
                outfx("final line from=%i: %s",
                        from,
                        lines[from] ? line_string(lines[from]) : "NULL"
                        );
                lines[to] = lines[from];
                to += 1;
            }
        }
        lines_num_old = lines_num;
        lines_num = to;
        if (extract_realloc2(
                &lines,
                sizeof(line_t*) * lines_num_old,
                sizeof(line_t*) * lines_num
                )) {
            /* Should always succeed because we're not increasing allocation size. */
            goto end;
        }
    }

    *o_lines = lines;
    *o_lines_num = lines_num;
    ret = 0;

    outf("Turned %i spans into %i lines. num_compatible=%i",
            spans_num,
            lines_num,
            num_compatible
            );

    end:
    if (ret) {
        /* Free everything. */
        if (lines) {
            for (a=0; a<lines_num; ++a) {
                if (lines[a])   extract_free(&lines[a]->spans);
                extract_free(&lines[a]);
            }
        }
        extract_free(&lines);
    }
    return ret;
}


/* Returns max font size of all span_t's in an line_t. */
static float line_font_size_max(line_t* line)
{
    float   size_max = 0;
    int i;
    for (i=0; i<line->spans_num; ++i) {
        span_t* span = line->spans[i];
        /* fixme: <size> should be float, which changes some output. */
        int size = (int) matrix_expansion(span->trm);
        if ((float) size > size_max) {
            size_max = (float) size;
        }
    }
    return size_max;
}



/* Find distance between parallel lines line_a and line_b, both at <angle>.

        _-R
     _-
    A------------_P
     \        _-
      \    _B
       \_-
        Q

A is (ax, ay)
B is (bx, by)
APB and PAR are both <angle>.

AR and QBP are parallel, and are the lines of text a and b
respectively.

AQB is a right angle. We need to find AQ.
*/
static float line_distance(
        float ax,
        float ay,
        float bx,
        float by,
        float angle
        )
{
    float dx = bx - ax;
    float dy = by - ay;


    return dx * sinf(angle) + dy * cosf(angle);
}


/* A comparison function for use with qsort(), for sorting paragraphs within a
page. */
static int paragraphs_cmp(const void* a, const void* b)
{
    paragraph_t* const* a_paragraph = a;
    paragraph_t* const* b_paragraph = b;
    line_t* a_line = paragraph_line_first(*a_paragraph);
    line_t* b_line = paragraph_line_first(*b_paragraph);

    span_t* a_span = line_span_first(a_line);
    span_t* b_span = line_span_first(b_line);

    /* If ctm matrices differ, always return this diff first. Note that we
    ignore .e and .f because if data is from ghostscript then .e and .f vary
    for each span, and we don't care about these differences. */
    int d = matrix_cmp4(&a_span->ctm, &b_span->ctm);
    if (d)  return d;

    {
        float a_angle = line_angle(a_line);
        float b_angle = line_angle(b_line);
        if (fabs(a_angle - b_angle) > 3.14/2) {
            /* Give up if more than 90 deg. */
            return 0;
        }
        {
            float angle = (a_angle + b_angle) / 2;
            float ax = line_item_first(a_line)->x;
            float ay = line_item_first(a_line)->y;
            float bx = line_item_first(b_line)->x;
            float by = line_item_first(b_line)->y;
            float distance = line_distance(ax, ay, bx, by, angle);
            if (distance > 0)   return -1;
            if (distance < 0)   return +1;
        }
    }
    return 0;
}


/* Creates a representation of line_t's that consists of a list of
paragraph_t's.

We only join lines that are at the same angle and are adjacent.

On entry:
    Original value of *o_paragraphs and *o_paragraphs_num are ignored.

    <lines> points to array of <lines_num> line_t*'s, each pointing to
    a line_t.

On exit:
    On sucess, returns zero, *o_paragraphs points to array of *o_paragraphs_num
    paragraph_t*'s, each pointing to an paragraph_t. In the
    array, paragraph_t's with same angle are sorted.

    On failure, returns -1 with errno set. *o_paragraphs and *o_paragraphs_num
    are undefined.
*/
static int make_paragraphs(
        line_t**        lines,
        int                     lines_num,
        paragraph_t***  o_paragraphs,
        int*                    o_paragraphs_num
        )
{
    int ret = -1;
    int a;
    int num_joins;
    paragraph_t** paragraphs = NULL;

    /* Start off with an paragraph_t for each line_t. */
    int paragraphs_num = lines_num;
    if (extract_malloc(&paragraphs, sizeof(*paragraphs) * paragraphs_num)) goto end;
    /* Ensure we can clean up after error when setting up. */
    for (a=0; a<paragraphs_num; ++a) {
        paragraphs[a] = NULL;
    }
    /* Set up initial paragraphs. */
    for (a=0; a<paragraphs_num; ++a) {
        if (extract_malloc(&paragraphs[a], sizeof(paragraph_t))) goto end;
        paragraphs[a]->lines_num = 0;
        if (extract_malloc(&paragraphs[a]->lines, sizeof(line_t*) * 1)) goto end;
        paragraphs[a]->lines_num = 1;
        paragraphs[a]->lines[0] = lines[a];
    }

    num_joins = 0;
    for (a=0; a<paragraphs_num; ++a) {
        paragraph_t* nearest_paragraph;
        int nearest_paragraph_b;
        float nearest_paragraph_distance;
        line_t* line_a;
        float angle_a;
        int verbose;
        int b;
        
        paragraph_t* paragraph_a = paragraphs[a];
        if (!paragraph_a) {
            /* This paragraph is empty - already been appended to a different
            paragraph. */
            continue;
        }

        nearest_paragraph = NULL;
        nearest_paragraph_b = -1;
        nearest_paragraph_distance = -1;
        assert(paragraph_a->lines_num > 0);

        line_a = paragraph_line_last(paragraph_a);
        angle_a = line_angle(line_a);

        verbose = 0;

        /* Look for nearest paragraph_t that could be appended to
        paragraph_a. */
        for (b=0; b<paragraphs_num; ++b) {
            paragraph_t* paragraph_b = paragraphs[b];
            line_t* line_b;
            if (!paragraph_b) {
                /* This paragraph is empty - already been appended to a different
                paragraph. */
                continue;
            }
            line_b = paragraph_line_first(paragraph_b);
            if (!lines_are_compatible(line_a, line_b, angle_a, 0)) {
                continue;
            }

            {
                float ax = line_item_last(line_a)->x;
                float ay = line_item_last(line_a)->y;
                float bx = line_item_first(line_b)->x;
                float by = line_item_first(line_b)->y;
                float distance = line_distance(ax, ay, bx, by, angle_a);
                if (verbose) {
                    outf(
                            "angle_a=%lf a=(%lf %lf) b=(%lf %lf) delta=(%lf %lf) distance=%lf:",
                            angle_a * 180 / g_pi,
                            ax, ay,
                            bx, by,
                            bx - ax,
                            by - ay,
                            distance
                            );
                    outf("    line_a=%s", line_string2(line_a));
                    outf("    line_b=%s", line_string2(line_b));
                }
                if (distance > 0) {
                    if (nearest_paragraph_distance == -1
                            || distance < nearest_paragraph_distance) {
                        if (verbose) {
                            outf("updating nearest. distance=%lf:", distance);
                            outf("    line_a=%s", line_string2(line_a));
                            outf("    line_b=%s", line_string2(line_b));
                        }
                        nearest_paragraph_distance = distance;
                        nearest_paragraph_b = b;
                        nearest_paragraph = paragraph_b;
                    }
                }
            }
        }

        if (nearest_paragraph) {
            float line_b_size = line_font_size_max(
                    paragraph_line_first(nearest_paragraph)
                    );
            line_t* line_b = paragraph_line_first(nearest_paragraph);
            (void) line_b; /* Only used in outfx(). */
            if (nearest_paragraph_distance < 1.5 * line_b_size) {
                span_t* a_span;
                int a_lines_num_new;
                if (verbose) {
                    outf(
                            "joing paragraphs. a=(%lf,%lf) b=(%lf,%lf) nearest_paragraph_distance=%lf line_b_size=%lf",
                            line_item_last(line_a)->x,
                            line_item_last(line_a)->y,
                            line_item_first(line_b)->x,
                            line_item_first(line_b)->y,
                            nearest_paragraph_distance,
                            line_b_size
                            );
                    outf("    %s", paragraph_string(paragraph_a));
                    outf("    %s", paragraph_string(nearest_paragraph));
                    outf("paragraph_a ctm=%s",
                            matrix_string(&paragraph_a->lines[0]->spans[0]->ctm)
                            );
                    outf("paragraph_a trm=%s",
                            matrix_string(&paragraph_a->lines[0]->spans[0]->trm)
                            );
                }
                /* Join these two paragraph_t's. */
                a_span = line_span_last(line_a);
                if (span_char_last(a_span)->ucs == '-') {
                    /* remove trailing '-' at end of prev line. char_t doesn't
                    contain any malloc-heap pointers so this doesn't leak. */
                    a_span->chars_num -= 1;
                }
                else {
                    /* Insert space before joining adjacent lines. */
                    char_t* c_prev;
                    char_t* c;
                    if (span_append_c(line_span_last(line_a), ' ')) goto end;
                    c_prev = &a_span->chars[ a_span->chars_num-2];
                    c = &a_span->chars[ a_span->chars_num-1];
                    c->x = c_prev->x + c_prev->adv * a_span->ctm.a;
                    c->y = c_prev->y + c_prev->adv * a_span->ctm.c;
                }

                a_lines_num_new = paragraph_a->lines_num + nearest_paragraph->lines_num;
                if (extract_realloc2(
                        &paragraph_a->lines,
                        sizeof(line_t*) * paragraph_a->lines_num,
                        sizeof(line_t*) * a_lines_num_new
                        )) goto end;
                {
                    int i;
                    for (i=0; i<nearest_paragraph->lines_num; ++i) {
                        paragraph_a->lines[paragraph_a->lines_num + i]
                            = nearest_paragraph->lines[i];
                    }
                }
                paragraph_a->lines_num = a_lines_num_new;

                /* Ensure that we skip nearest_paragraph in future. */
                extract_free(&nearest_paragraph->lines);
                extract_free(&nearest_paragraph);
                paragraphs[nearest_paragraph_b] = NULL;

                num_joins += 1;
                outfx(
                        "have joined paragraph a=%i to snearest_paragraph_b=%i",
                        a,
                        nearest_paragraph_b
                        );

                if (nearest_paragraph_b > a) {
                    /* We haven't yet tried appending any paragraphs to
                    nearest_paragraph_b, so the new extended paragraph_a needs
                    checking again. */
                    a -= 1;
                }
            }
            else {
                outfx(
                        "Not joining paragraphs. nearest_paragraph_distance=%lf line_b_size=%lf",
                        nearest_paragraph_distance,
                        line_b_size
                        );
            }
        }
    }

    {
        /* Remove empty paragraphs. */
        int from;
        int to;
        int paragraphs_num_old;
        for (from=0, to=0; from<paragraphs_num; ++from) {
            if (paragraphs[from]) {
                paragraphs[to] = paragraphs[from];
                to += 1;
            }
        }
        outfx("paragraphs_num=%i => %i", paragraphs_num, to);
        paragraphs_num_old = paragraphs_num;
        paragraphs_num = to;
        if (extract_realloc2(
                &paragraphs,
                sizeof(paragraph_t*) * paragraphs_num_old,
                sizeof(paragraph_t*) * paragraphs_num
                )) {
            /* Should always succeed because we're not increasing allocation size, but
            can fail with memento squeeze. */
            goto end;
        }
    }

    /* Sort paragraphs so they appear in correct order, using paragraphs_cmp().
    */
    qsort(
            paragraphs,
            paragraphs_num,
            sizeof(paragraph_t*), paragraphs_cmp
            );

    *o_paragraphs = paragraphs;
    *o_paragraphs_num = paragraphs_num;
    ret = 0;
    outf("Turned %i lines into %i paragraphs",
            lines_num,
            paragraphs_num
            );


    end:

    if (ret) {
        if (paragraphs) {
            for (a=0; a<paragraphs_num; ++a) {
                if (paragraphs[a])   extract_free(&paragraphs[a]->lines);
                extract_free(&paragraphs[a]);
            }
        }
        extract_free(&paragraphs);
    }
    return ret;
}


static void s_document_init(extract_document_t* document)
{
    document->pages = NULL;
    document->pages_num = 0;
}

/* Does preliminary processing of the end of the last spen in a page; intended
to be called as we load span information.

Looks at last two char_t's in last span_t of <page>, and either
leaves unchanged, or removes space in last-but-one position, or moves last
char_t into a new span_t. */
static int page_span_end_clean(page_t* page)
{
    int ret = -1;
    span_t* span;
    char_t* char_;
    float font_size;
    float x;
    float y;
    float err_x;
    float err_y;
    point_t dir;
    
    assert(page->spans_num);
    span = page->spans[page->spans_num-1];
    assert(span->chars_num);

    /* Last two char_t's are char_[-2] and char_[-1]. */
    char_ = &span->chars[span->chars_num];

    if (span->chars_num == 1) {
        return 0;
    }

    font_size = matrix_expansion(span->trm)
            * matrix_expansion(span->ctm);

    if (span->wmode) {
        dir.x = 0;
        dir.y = 1;
    }
    else {
        dir.x = 1;
        dir.y = 0;
    }
    dir = multiply_matrix_point(span->trm, dir);

    x = char_[-2].pre_x + char_[-2].adv * dir.x;
    y = char_[-2].pre_y + char_[-2].adv * dir.y;

    err_x = (char_[-1].pre_x - x) / font_size;
    err_y = (char_[-1].pre_y - y) / font_size;

    if (span->chars_num >= 2 && span->chars[span->chars_num-2].ucs == ' ') {
        int remove_penultimate_space = 0;
        if (err_x < -span->chars[span->chars_num-2].adv / 2
                && err_x > -span->chars[span->chars_num-2].adv
                ) {
            remove_penultimate_space = 1;
        }
        if ((char_[-1].pre_x - char_[-2].pre_x) / font_size < char_[-1].adv / 10) {
            outfx(
                    "removing penultimate space because space very narrow:"
                    "char_[-1].pre_x-char_[-2].pre_x=%f font_size=%f"
                    " char_[-1].adv=%f",
                    char_[-1].pre_x - char_[-2].pre_x,
                    font_size,
                    char_[-1].adv
                    );
            remove_penultimate_space = 1;
        }
        if (remove_penultimate_space) {
            /* This character overlaps with previous space
            character. We discard previous space character - these
            sometimes seem to appear in the middle of words for some
            reason. */
            outfx("removing space before final char in: %s",
                    span_string(span));
            span->chars[span->chars_num-2] = span->chars[span->chars_num-1];
            span->chars_num -= 1;
            outfx("span is now:                         %s", span_string(span));
            return 0;
        }
    }
    else if (fabs(err_x) > 0.01 || fabs(err_y) > 0.01) {
        /* This character doesn't seem to be a continuation of
        previous characters, so split into two spans. This often
        splits text incorrectly, but this is corrected later when
        we join spans into lines. */
        outfx(
                "Splitting last char into new span. font_size=%f dir.x=%f"
                " char[-1].pre=(%f, %f) err=(%f, %f): %s",
                font_size,
                dir.x,
                char_[-1].pre_x,
                char_[-1].pre_y,
                err_x,
                err_y,
                span_string2(span)
                );
        {
            span_t* span2 = page_span_append(page);
            if (!span2) goto end;
            *span2 = *span;
            if (extract_strdup(span->font_name, &span2->font_name)) goto end;
            span2->chars_num = 1;
            if (extract_malloc(&span2->chars, sizeof(char_t) * span2->chars_num)) goto end;
            span2->chars[0] = char_[-1];
            span->chars_num -= 1;
        }
        return 0;
    }
    ret = 0;
    end:
    return ret;
}


int extract_intermediate_to_document(
        extract_buffer_t*       buffer,
        int                     autosplit,
        extract_document_t**    o_document
        )
{
    int ret = -1;

    extract_document_t* document = NULL;
    image_t image_temp = {0};
    
    int num_spans = 0;

    /* Number of extra spans from page_span_end_clean(). */
    int num_spans_split = 0;

    /* Num extra spans from autosplit=1. */
    int num_spans_autosplit = 0;

    extract_xml_tag_t   tag;
    extract_xml_tag_init(&tag);

    if (extract_malloc(&document, sizeof(**o_document))) goto end;
    s_document_init(document);
    
    if (extract_xml_pparse_init(buffer, NULL /*first_line*/)) {
        outf("Failed to read start of intermediate data: %s", strerror(errno));
        goto end;
    }
    /* Data read from <path> is expected to be XML looking like:

    <page>
        <span>
            <char ...>
            <char ...>
            ...
        </span>
        <span>
            ...
        </span>
        ...
    </page>
    <page>
        ...
    </page>
    ...

    We convert this into a list of page_t's, each containing a list of
    span_t's, each containing a list of char_t's.

    While doing this, we do some within-span processing by calling
    page_span_end_clean():
        Remove spurious spaces.
        Split spans in two where there seem to be large gaps between glyphs.
    */
    for(;;) {
        page_t* page;
        int e = extract_xml_pparse_next(buffer, &tag);
        if (e == 1) break; /* EOF. */
        if (e) goto end;
        if (!strcmp(tag.name, "?xml")) {
            /* We simply skip this if we find it. As of 2020-07-31, mutool adds
            this header to mupdf raw output, but gs txtwrite does not include
            it. */
            continue;
        }
        if (strcmp(tag.name, "page")) {
            outf("Expected <page> but tag.name='%s'", tag.name);
            errno = ESRCH;
            goto end;
        }
        outfx("loading spans for page %i...", document->pages_num);
        page = document_page_append(document);
        if (!page) goto end;

        for(;;) {
            if (extract_xml_pparse_next(buffer, &tag)) goto end;
            if (!strcmp(tag.name, "/page")) {
                num_spans += page->spans_num;
                break;
            }
            if (!strcmp(tag.name, "image")) {
                /* For now we simply skip images. */
                const char* type = extract_xml_tag_attributes_find(&tag, "type");
                if (!type) {
                    errno = EINVAL;
                    goto end;
                }
                if (!strcmp(type, "pixmap")) {
                    int w;
                    int h;
                    int y;
                    if (extract_xml_tag_attributes_find_int(&tag, "w", &w)) goto end;
                    if (extract_xml_tag_attributes_find_int(&tag, "h", &h)) goto end;
                    for (y=0; y<h; ++y) {
                        int yy;
                        if (extract_xml_pparse_next(buffer, &tag)) goto end;
                        if (strcmp(tag.name, "line")) {
                            outf("Expected <line> but tag.name='%s'", tag.name);
                            errno = ESRCH;
                            goto end;
                        }
                        if (extract_xml_tag_attributes_find_int(&tag, "y", &yy)) goto end;
                        if (yy != y) {
                            outf("Expected <line y=%i> but found <line y=%i>", y, yy);
                            errno = ESRCH;
                            goto end;
                        }
                        if (extract_xml_pparse_next(buffer, &tag)) goto end;
                        if (strcmp(tag.name, "/line")) {
                            outf("Expected </line> but tag.name='%s'", tag.name);
                            errno = ESRCH;
                            goto end;
                        }
                    }
                }
                else {
                    /* Compressed. */
                    
                    /* We use a static here to ensure uniqueness. Start at 10
                    because template document might use some low-numbered IDs.
                    */
                    static int  s_image_n = 10;
                    size_t  i = 0;
                    const char* c = tag.text.chars;
                    const char* type = extract_xml_tag_attributes_find(&tag, "type");
                    if (!type) goto end;
                    
                    assert(!image_temp.type);
                    assert(!image_temp.data);
                    assert(!image_temp.data_size);
                    
                    if (extract_strdup(type, &image_temp.type)) goto end;
                    if (extract_xml_tag_attributes_find_size(&tag, "datasize", &image_temp.data_size)) goto end;
                    if (extract_malloc(&image_temp.data, image_temp.data_size)) goto end;
                    s_image_n += 1;
                    {
                        char id_text[32];
                        snprintf(id_text, sizeof(id_text), "rId%i", s_image_n);
                        if (extract_strdup(id_text, &image_temp.id)) goto end;
                    }
                    if (!image_temp.id) goto end;
                    if (extract_asprintf(&image_temp.name, "image%i.%s", s_image_n, image_temp.type) < 0) goto end;
                    
                    outfx("type=%s: image_temp.type=%s image_temp.data_size=%zi image_temp.name=%s image_temp.id=%s",
                            type, image_temp.type, image_temp.data_size, image_temp.name, image_temp.id);
                    
                    for(i=0;;) {
                        int byte = 0;
                        int cc;
                        cc = *c;
                        c += 1;
                        if (cc == ' ' || cc == '\n') continue;
                        if (cc >= '0' && cc <= '9') byte += cc-'0';
                        else if (cc >= 'a' && cc <= 'f') byte += 10 + cc - 'a';
                        else goto compressed_error;
                        byte *= 16;
                        
                        cc = *c;
                        c += 1;
                        if (cc >= '0' && cc <= '9') byte += cc-'0';
                        else if (cc >= 'a' && cc <= 'f') byte += 10 + cc - 'a';
                        else goto compressed_error;
                        
                        image_temp.data[i] = (char) byte;
                        i += 1;
                        if (i == image_temp.data_size) {
                            break;
                        }
                        continue;
                        
                        compressed_error:
                        outf("Unrecognised hex character '%x' at offset %lli in image data", cc, (long long) (c-tag.text.chars));
                        errno = EINVAL;
                        goto end;
                    }
                    if (extract_realloc2(
                            &page->images,
                            sizeof(image_t) * page->images_num,
                            sizeof(image_t) * (page->images_num + 1)
                            )) goto end;
                    page->images[page->images_num].name = image_temp.name;
                    page->images[page->images_num].id = image_temp.id;
                    page->images[page->images_num].type = image_temp.type;
                    page->images[page->images_num].data = image_temp.data;
                    page->images[page->images_num].data_size = image_temp.data_size;
                    outf("type=%s image_temp.type=%s image_temp.data=%i %i %i %i",
                            type, image_temp.type,
                            image_temp.data[0],
                            image_temp.data[1],
                            image_temp.data[2],
                            image_temp.data[3]
                            );
                    page->images_num += 1;
                    
                    /* Ensure out cleanup does not free data that is now in page->images[]. */
                    image_temp.name = NULL;
                    image_temp.type = NULL;
                    image_temp.id = NULL;
                    image_temp.data = NULL;
                    image_temp.data_size = 0;
                }
                if (extract_xml_pparse_next(buffer, &tag)) goto end;
                if (strcmp(tag.name, "/image")) {
                    outf("Expected </image> but tag.name='%s'", tag.name);
                    errno = ESRCH;
                    goto end;
                }
                continue;
            }
            if (strcmp(tag.name, "span")) {
                outf("Expected <span> but tag.name='%s'", tag.name);
                errno = ESRCH;
                goto end;
            }

            {
                span_t* span = page_span_append(page);
                char*   f;
                char*   ff;
                float   offset_x;
                float   offset_y;
                
                if (!span) goto end;

                if (s_matrix_read(extract_xml_tag_attributes_find(&tag, "ctm"), &span->ctm)) goto end;
                if (s_matrix_read(extract_xml_tag_attributes_find(&tag, "trm"), &span->trm)) goto end;

                f = extract_xml_tag_attributes_find(&tag, "font_name");
                if (!f) {
                    outf("Failed to find attribute 'font_name'");
                    goto end;
                }
                ff = strchr(f, '+');
                if (ff)  f = ff + 1;
                if (extract_strdup(f, &span->font_name)) goto end;
                span->font_bold = strstr(span->font_name, "-Bold") ? 1 : 0;
                span->font_italic = strstr(span->font_name, "-Oblique") ? 1 : 0;

                {
                    /* Need to use temporary int because span->wmode is a bitfield. */
                    int wmode;
                    if (extract_xml_tag_attributes_find_int(&tag, "wmode", &wmode)) {
                        outf("Failed to find attribute 'wmode'");
                        goto end;
                    }
                    span->wmode = wmode;
                }

                offset_x = 0;
                offset_y = 0;
                for(;;) {
                    float char_pre_x;
                    float char_pre_y;
                    char_t* char_;

                    if (extract_xml_pparse_next(buffer, &tag)) {
                        outf("Failed to find <char or </span");
                        goto end;
                    }
                    if (!strcmp(tag.name, "/span")) {
                        break;
                    }
                    if (strcmp(tag.name, "char")) {
                        errno = ESRCH;
                        outf("Expected <char> but tag.name='%s'", tag.name);
                        goto end;
                    }

                    if (extract_xml_tag_attributes_find_float(&tag, "x", &char_pre_x)) goto end;
                    if (extract_xml_tag_attributes_find_float(&tag, "y", &char_pre_y)) goto end;

                    if (autosplit && char_pre_y - offset_y != 0) {
                        float e = span->ctm.e + span->ctm.a * (char_pre_x-offset_x)
                                + span->ctm.b * (char_pre_y - offset_y);
                        float f = span->ctm.f + span->ctm.c * (char_pre_x-offset_x)
                                + span->ctm.d * (char_pre_y - offset_y);
                        offset_x = char_pre_x;
                        offset_y = char_pre_y;
                        outfx("autosplit: char_pre_y=%f offset_y=%f",
                                char_pre_y, offset_y);
                        outfx(
                                "autosplit: changing ctm.{e,f} from (%f, %f) to (%f, %f)",
                                span->ctm.e,
                                span->ctm.f,
                                e, f
                                );
                        if (span->chars_num > 0) {
                            /* Create new span. */
                            span_t* span0 = span;
                            num_spans_autosplit += 1;
                            span = page_span_append(page);
                            if (!span) goto end;
                            *span = *span0;
                            span->chars = NULL;
                            span->chars_num = 0;
                            if (extract_strdup(span0->font_name, &span->font_name)) goto end;
                        }
                        span->ctm.e = e;
                        span->ctm.f = f;
                        outfx("autosplit: char_pre_y=%f offset_y=%f",
                                char_pre_y, offset_y);
                    }

                    if (span_append_c(span, 0 /*c*/)) goto end;
                    char_ = &span->chars[ span->chars_num-1];
                    char_->pre_x = char_pre_x - offset_x;
                    char_->pre_y = char_pre_y - offset_y;
                    if (char_->pre_y != 0) {
                        outfx("char_->pre=(%f %f)",
                                char_->pre_x, char_->pre_y);
                    }

                    char_->x = span->ctm.a * char_->pre_x + span->ctm.b * char_->pre_y;
                    char_->y = span->ctm.c * char_->pre_x + span->ctm.d * char_->pre_y;

                    if (extract_xml_tag_attributes_find_float(&tag, "adv", &char_->adv)) goto end;

                    if (extract_xml_tag_attributes_find_uint(&tag, "ucs", &char_->ucs)) goto end;

                    {
                        char    trm[64];
                        snprintf(trm, sizeof(trm), "%s", matrix_string(&span->trm));
                    }
                    char_->x += span->ctm.e;
                    char_->y += span->ctm.f;

                    outfx(
                            "ctm=%s trm=%s ctm*trm=%f pre=(%f %f) =>"
                            " xy=(%f %f) [orig xy=(%f %f)]",
                            matrix_string(&span->ctm),
                            trm,
                            span->ctm.a * span->trm.a,
                            char_->pre_x,
                            char_->pre_y,
                            char_->x,
                            char_->y,
                            x, y
                            );

                    {
                        int page_spans_num_old = page->spans_num;
                        if (page_span_end_clean(page)) goto end;
                        span = page->spans[page->spans_num-1];
                        if (page->spans_num != page_spans_num_old) {
                            num_spans_split += 1;
                        }
                    }
                }
                extract_xml_tag_free(&tag);
            }
        }
        outf("page=%i page->num_spans=%i",
                document->pages_num, page->spans_num);
    }

    outf("num_spans=%i num_spans_split=%i num_spans_autosplit=%i",
            num_spans,
            num_spans_split,
            num_spans_autosplit
            );

    ret = 0;

    end:
    extract_xml_tag_free(&tag);
    
    extract_free(&image_temp.type);
    extract_free(&image_temp.data);
    extract_free(&image_temp.id);
    extract_free(&image_temp.name);
    image_temp.type = NULL;
    image_temp.data = NULL;
    image_temp.data_size = 0;

    if (ret) {
        outf("read_spans_raw() returning error ret=%i", ret);
        extract_document_free(&document);
        *o_document = NULL;
    }
    else {
        *o_document = document;
    }

    return ret;
}


static float matrices_to_font_size(matrix_t* ctm, matrix_t* trm)
{
    float font_size = matrix_expansion(*trm)
            * matrix_expansion(*ctm);
    /* Round font_size to nearest 0.01. */
    font_size = (float) (int) (font_size * 100.0f + 0.5f) / 100.0f;
    return font_size;
}

typedef struct
{
    const char* font_name;
    float       font_size;
    int         font_bold;
    int         font_italic;
    matrix_t*   ctm_prev;
} content_state_t;


static int extract_document_to_docx_content_paragraph(
        content_state_t*    state,
        paragraph_t*        paragraph,
        extract_astring_t*  content
        )
{
    int e = -1;
    int l;
    if (extract_docx_paragraph_start(content)) goto end;

    for (l=0; l<paragraph->lines_num; ++l) {
        line_t* line = paragraph->lines[l];
        int s;
        for (s=0; s<line->spans_num; ++s) {
            int si;
            span_t* span = line->spans[s];
            float font_size_new;
            state->ctm_prev = &span->ctm;
            font_size_new = matrices_to_font_size(&span->ctm, &span->trm);
            if (!state->font_name
                    || strcmp(span->font_name, state->font_name)
                    || span->font_bold != state->font_bold
                    || span->font_italic != state->font_italic
                    || font_size_new != state->font_size
                    ) {
                if (state->font_name) {
                    if (extract_docx_run_finish(content)) goto end;
                }
                state->font_name = span->font_name;
                state->font_bold = span->font_bold;
                state->font_italic = span->font_italic;
                state->font_size = font_size_new;
                if (extract_docx_run_start(
                        content,
                        state->font_name,
                        state->font_size,
                        state->font_bold,
                        state->font_italic
                        )) goto end;
            }

            for (si=0; si<span->chars_num; ++si) {
                char_t* char_ = &span->chars[si];
                int c = char_->ucs;

                if (0) {}

                /* Escape XML special characters. */
                else if (c == '<')  extract_docx_char_append_string(content, "&lt;");
                else if (c == '>')  extract_docx_char_append_string(content, "&gt;");
                else if (c == '&')  extract_docx_char_append_string(content, "&amp;");
                else if (c == '"')  extract_docx_char_append_string(content, "&quot;");
                else if (c == '\'') extract_docx_char_append_string(content, "&apos;");

                /* Expand ligatures. */
                else if (c == 0xFB00) {
                    if (extract_docx_char_append_string(content, "ff")) goto end;
                }
                else if (c == 0xFB01) {
                    if (extract_docx_char_append_string(content, "fi")) goto end;
                }
                else if (c == 0xFB02) {
                    if (extract_docx_char_append_string(content, "fl")) goto end;
                }
                else if (c == 0xFB03) {
                    if (extract_docx_char_append_string(content, "ffi")) goto end;
                }
                else if (c == 0xFB04) {
                    if (extract_docx_char_append_string(content, "ffl")) goto end;
                }

                /* Output ASCII verbatim. */
                else if (c >= 32 && c <= 127) {
                    if (extract_docx_char_append_char(content, (char) c)) goto end;
                }

                /* Escape all other characters. */
                else {
                    char    buffer[32];
                    snprintf(buffer, sizeof(buffer), "&#x%x;", c);
                    if (extract_docx_char_append_string(content, buffer)) goto end;
                }
            }
            /* Remove any trailing '-' at end of line. */
            if (extract_docx_char_truncate_if(content, '-')) goto end;
        }
    }
    if (state->font_name) {
        if (extract_docx_run_finish(content)) goto end;
        state->font_name = NULL;
    }
    if (extract_docx_paragraph_finish(content)) goto end;
    
    e = 0;
    
    end:
    return e;
}

static int extract_document_append_image(
        extract_astring_t*  content,
        image_t*            image
        )
/* Write reference to image into docx content. */
{
    extract_docx_char_append_string(content, "\n");
    extract_docx_char_append_string(content, "     <w:p>\n");
    extract_docx_char_append_string(content, "       <w:r>\n");
    extract_docx_char_append_string(content, "         <w:rPr>\n");
    extract_docx_char_append_string(content, "           <w:noProof/>\n");
    extract_docx_char_append_string(content, "         </w:rPr>\n");
    extract_docx_char_append_string(content, "         <w:drawing>\n");
    extract_docx_char_append_string(content, "           <wp:inline distT=\"0\" distB=\"0\" distL=\"0\" distR=\"0\" wp14:anchorId=\"7057A832\" wp14:editId=\"466EB3FB\">\n");
    extract_docx_char_append_string(content, "             <wp:extent cx=\"2933700\" cy=\"2200275\"/>\n");
    extract_docx_char_append_string(content, "             <wp:effectExtent l=\"0\" t=\"0\" r=\"0\" b=\"9525\"/>\n");
    extract_docx_char_append_string(content, "             <wp:docPr id=\"1\" name=\"Picture 1\"/>\n");
    extract_docx_char_append_string(content, "             <wp:cNvGraphicFramePr>\n");
    extract_docx_char_append_string(content, "               <a:graphicFrameLocks xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" noChangeAspect=\"1\"/>\n");
    extract_docx_char_append_string(content, "             </wp:cNvGraphicFramePr>\n");
    extract_docx_char_append_string(content, "             <a:graphic xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\">\n");
    extract_docx_char_append_string(content, "               <a:graphicData uri=\"http://schemas.openxmlformats.org/drawingml/2006/picture\">\n");
    extract_docx_char_append_string(content, "                 <pic:pic xmlns:pic=\"http://schemas.openxmlformats.org/drawingml/2006/picture\">\n");
    extract_docx_char_append_string(content, "                   <pic:nvPicPr>\n");
    extract_docx_char_append_string(content, "                     <pic:cNvPr id=\"1\" name=\"Picture 1\"/>\n");
    extract_docx_char_append_string(content, "                     <pic:cNvPicPr>\n");
    extract_docx_char_append_string(content, "                       <a:picLocks noChangeAspect=\"1\" noChangeArrowheads=\"1\"/>\n");
    extract_docx_char_append_string(content, "                     </pic:cNvPicPr>\n");
    extract_docx_char_append_string(content, "                   </pic:nvPicPr>\n");
    extract_docx_char_append_string(content, "                   <pic:blipFill>\n");
    extract_docx_char_append_stringf(content,"                     <a:blip r:embed=\"%s\">\n", image->id);
    extract_docx_char_append_string(content, "                       <a:extLst>\n");
    extract_docx_char_append_string(content, "                         <a:ext uri=\"{28A0092B-C50C-407E-A947-70E740481C1C}\">\n");
    extract_docx_char_append_string(content, "                           <a14:useLocalDpi xmlns:a14=\"http://schemas.microsoft.com/office/drawing/2010/main\" val=\"0\"/>\n");
    extract_docx_char_append_string(content, "                         </a:ext>\n");
    extract_docx_char_append_string(content, "                       </a:extLst>\n");
    extract_docx_char_append_string(content, "                     </a:blip>\n");
    //extract_docx_char_append_string(content, "                     <a:srcRect/>\n");
    extract_docx_char_append_string(content, "                     <a:stretch>\n");
    extract_docx_char_append_string(content, "                       <a:fillRect/>\n");
    extract_docx_char_append_string(content, "                     </a:stretch>\n");
    extract_docx_char_append_string(content, "                   </pic:blipFill>\n");
    extract_docx_char_append_string(content, "                   <pic:spPr bwMode=\"auto\">\n");
    extract_docx_char_append_string(content, "                     <a:xfrm>\n");
    extract_docx_char_append_string(content, "                       <a:off x=\"0\" y=\"0\"/>\n");
    extract_docx_char_append_string(content, "                       <a:ext cx=\"2933700\" cy=\"2200275\"/>\n");
    extract_docx_char_append_string(content, "                     </a:xfrm>\n");
    extract_docx_char_append_string(content, "                     <a:prstGeom prst=\"rect\">\n");
    extract_docx_char_append_string(content, "                       <a:avLst/>\n");
    extract_docx_char_append_string(content, "                     </a:prstGeom>\n");
    extract_docx_char_append_string(content, "                     <a:noFill/>\n");
    extract_docx_char_append_string(content, "                     <a:ln>\n");
    extract_docx_char_append_string(content, "                       <a:noFill/>\n");
    extract_docx_char_append_string(content, "                     </a:ln>\n");
    extract_docx_char_append_string(content, "                   </pic:spPr>\n");
    extract_docx_char_append_string(content, "                 </pic:pic>\n");
    extract_docx_char_append_string(content, "               </a:graphicData>\n");
    extract_docx_char_append_string(content, "             </a:graphic>\n");
    extract_docx_char_append_string(content, "           </wp:inline>\n");
    extract_docx_char_append_string(content, "         </w:drawing>\n");
    extract_docx_char_append_string(content, "       </w:r>\n");
    extract_docx_char_append_string(content, "     </w:p>\n");
    extract_docx_char_append_string(content, "\n");
    return 0;
}


static int extract_document_output_rotated_paragraphs(
        page_t*             page,
        int                 paragraph_begin,
        int                 paragraph_end,
        int                 rot,
        int                 x,
        int                 y,
        int                 w,
        int                 h,
        int                 text_box_id,
        extract_astring_t*  content,
        content_state_t*    state
        )
/* Writes paragraph to content inside rotated text box. */
{
    int e = 0;
    outf("x,y=%ik,%ik = %i,%i", x/1000, y/1000, x, y);
    extract_docx_char_append_string(content, "\n");
    extract_docx_char_append_string(content, "\n");
    extract_docx_char_append_string(content, "<w:p>\n");
    extract_docx_char_append_string(content, "  <w:r>\n");
    extract_docx_char_append_string(content, "    <mc:AlternateContent>\n");
    extract_docx_char_append_string(content, "      <mc:Choice Requires=\"wps\">\n");
    extract_docx_char_append_string(content, "        <w:drawing>\n");
    extract_docx_char_append_string(content, "          <wp:anchor distT=\"0\" distB=\"0\" distL=\"0\" distR=\"0\" simplePos=\"0\" relativeHeight=\"0\" behindDoc=\"0\" locked=\"0\" layoutInCell=\"1\" allowOverlap=\"1\" wp14:anchorId=\"53A210D1\" wp14:editId=\"2B7E8016\">\n");
    extract_docx_char_append_string(content, "            <wp:simplePos x=\"0\" y=\"0\"/>\n");
    extract_docx_char_append_string(content, "            <wp:positionH relativeFrom=\"page\">\n");
    extract_docx_char_append_stringf(content,"              <wp:posOffset>%i</wp:posOffset>\n", x);
    extract_docx_char_append_string(content, "            </wp:positionH>\n");
    extract_docx_char_append_string(content, "            <wp:positionV relativeFrom=\"page\">\n");
    extract_docx_char_append_stringf(content,"              <wp:posOffset>%i</wp:posOffset>\n", y);
    extract_docx_char_append_string(content, "            </wp:positionV>\n");
    extract_docx_char_append_stringf(content,"            <wp:extent cx=\"%i\" cy=\"%i\"/>\n", w, h);
    extract_docx_char_append_string(content, "            <wp:effectExtent l=\"381000\" t=\"723900\" r=\"371475\" b=\"723900\"/>\n");
    extract_docx_char_append_string(content, "            <wp:wrapNone/>\n");
    extract_docx_char_append_stringf(content,"            <wp:docPr id=\"%i\" name=\"Text Box %i\"/>\n", text_box_id, text_box_id);
    extract_docx_char_append_string(content, "            <wp:cNvGraphicFramePr/>\n");
    extract_docx_char_append_string(content, "            <a:graphic xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\">\n");
    extract_docx_char_append_string(content, "              <a:graphicData uri=\"http://schemas.microsoft.com/office/word/2010/wordprocessingShape\">\n");
    extract_docx_char_append_string(content, "                <wps:wsp>\n");
    extract_docx_char_append_string(content, "                  <wps:cNvSpPr txBox=\"1\"/>\n");
    extract_docx_char_append_string(content, "                  <wps:spPr>\n");
    extract_docx_char_append_stringf(content,"                    <a:xfrm rot=\"%i\">\n", rot);
    extract_docx_char_append_string(content, "                      <a:off x=\"0\" y=\"0\"/>\n");
    extract_docx_char_append_string(content, "                      <a:ext cx=\"3228975\" cy=\"2286000\"/>\n");
    extract_docx_char_append_string(content, "                    </a:xfrm>\n");
    extract_docx_char_append_string(content, "                    <a:prstGeom prst=\"rect\">\n");
    extract_docx_char_append_string(content, "                      <a:avLst/>\n");
    extract_docx_char_append_string(content, "                    </a:prstGeom>\n");

    /* Give box a solid background. */
    if (0) {
        extract_docx_char_append_string(content, "                    <a:solidFill>\n");
        extract_docx_char_append_string(content, "                      <a:schemeClr val=\"lt1\"/>\n");
        extract_docx_char_append_string(content, "                    </a:solidFill>\n");
        }

    /* Draw line around box. */
    if (0) {
        extract_docx_char_append_string(content, "                    <a:ln w=\"175\">\n");
        extract_docx_char_append_string(content, "                      <a:solidFill>\n");
        extract_docx_char_append_string(content, "                        <a:prstClr val=\"black\"/>\n");
        extract_docx_char_append_string(content, "                      </a:solidFill>\n");
        extract_docx_char_append_string(content, "                    </a:ln>\n");
    }

    extract_docx_char_append_string(content, "                  </wps:spPr>\n");
    extract_docx_char_append_string(content, "                  <wps:txbx>\n");
    extract_docx_char_append_string(content, "                    <w:txbxContent>");

    #if 0
    if (0) {
        /* Output inline text describing the rotation. */
        extract_docx_char_append_stringf(content, "<w:p>\n"
                "<w:r><w:rPr><w:rFonts w:ascii=\"OpenSans\" w:hAnsi=\"OpenSans\"/><w:sz w:val=\"20.000000\"/><w:szCs w:val=\"15.000000\"/></w:rPr><w:t xml:space=\"preserve\">*** rotate: %f rad, %f deg. rot=%i</w:t></w:r>\n"
                "</w:p>\n",
                rotate,
                rotate * 180 / g_pi,
                rot
                );
    }
    #endif

    /* Output paragraphs p0..p2-1. */
    for (int p=paragraph_begin; p<paragraph_end; ++p) {
        paragraph_t* paragraph = page->paragraphs[p];
        if (extract_document_to_docx_content_paragraph(state, paragraph, content)) goto end;
    }

    extract_docx_char_append_string(content, "\n");
    extract_docx_char_append_string(content, "                    </w:txbxContent>\n");
    extract_docx_char_append_string(content, "                  </wps:txbx>\n");
    extract_docx_char_append_string(content, "                  <wps:bodyPr rot=\"0\" spcFirstLastPara=\"0\" vertOverflow=\"overflow\" horzOverflow=\"overflow\" vert=\"horz\" wrap=\"square\" lIns=\"91440\" tIns=\"45720\" rIns=\"91440\" bIns=\"45720\" numCol=\"1\" spcCol=\"0\" rtlCol=\"0\" fromWordArt=\"0\" anchor=\"t\" anchorCtr=\"0\" forceAA=\"0\" compatLnSpc=\"1\">\n");
    extract_docx_char_append_string(content, "                    <a:prstTxWarp prst=\"textNoShape\">\n");
    extract_docx_char_append_string(content, "                      <a:avLst/>\n");
    extract_docx_char_append_string(content, "                    </a:prstTxWarp>\n");
    extract_docx_char_append_string(content, "                    <a:noAutofit/>\n");
    extract_docx_char_append_string(content, "                  </wps:bodyPr>\n");
    extract_docx_char_append_string(content, "                </wps:wsp>\n");
    extract_docx_char_append_string(content, "              </a:graphicData>\n");
    extract_docx_char_append_string(content, "            </a:graphic>\n");
    extract_docx_char_append_string(content, "          </wp:anchor>\n");
    extract_docx_char_append_string(content, "        </w:drawing>\n");
    extract_docx_char_append_string(content, "      </mc:Choice>\n");

    /* This fallack is copied from a real Word document. Not sure
    whether it works - both Libreoffice and Word use the above
    choice. */
    extract_docx_char_append_string(content, "      <mc:Fallback>\n");
    extract_docx_char_append_string(content, "        <w:pict>\n");
    extract_docx_char_append_string(content, "          <v:shapetype w14:anchorId=\"53A210D1\" id=\"_x0000_t202\" coordsize=\"21600,21600\" o:spt=\"202\" path=\"m,l,21600r21600,l21600,xe\">\n");
    extract_docx_char_append_string(content, "            <v:stroke joinstyle=\"miter\"/>\n");
    extract_docx_char_append_string(content, "            <v:path gradientshapeok=\"t\" o:connecttype=\"rect\"/>\n");
    extract_docx_char_append_string(content, "          </v:shapetype>\n");
    extract_docx_char_append_stringf(content,"          <v:shape id=\"Text Box %i\" o:spid=\"_x0000_s1026\" type=\"#_x0000_t202\" style=\"position:absolute;margin-left:71.25pt;margin-top:48.75pt;width:254.25pt;height:180pt;rotation:-2241476fd;z-index:251659264;visibility:visible;mso-wrap-style:square;mso-wrap-distance-left:9pt;mso-wrap-distance-top:0;mso-wrap-distance-right:9pt;mso-wrap-distance-bottom:0;mso-position-horizontal:absolute;mso-position-horizontal-relative:text;mso-position-vertical:absolute;mso-position-vertical-relative:text;v-text-anchor:top\" o:gfxdata=\"UEsDBBQABgAIAAAAIQC2gziS/gAAAOEBAAATAAAAW0NvbnRlbnRfVHlwZXNdLnhtbJSRQU7DMBBF&#10;90jcwfIWJU67QAgl6YK0S0CoHGBkTxKLZGx5TGhvj5O2G0SRWNoz/78nu9wcxkFMGNg6quQqL6RA&#10;0s5Y6ir5vt9lD1JwBDIwOMJKHpHlpr69KfdHjyxSmriSfYz+USnWPY7AufNIadK6MEJMx9ApD/oD&#10;OlTrorhX2lFEilmcO2RdNtjC5xDF9pCuTyYBB5bi6bQ4syoJ3g9WQ0ymaiLzg5KdCXlKLjvcW893&#10;SUOqXwnz5DrgnHtJTxOsQfEKIT7DmDSUCaxw7Rqn8787ZsmRM9e2VmPeBN4uqYvTtW7jvijg9N/y&#10;JsXecLq0q+WD6m8AAAD//wMAUEsDBBQABgAIAAAAIQA4/SH/1gAAAJQBAAALAAAAX3JlbHMvLnJl&#10;bHOkkMFqwzAMhu+DvYPRfXGawxijTi+j0GvpHsDYimMaW0Yy2fr2M4PBMnrbUb/Q94l/f/hMi1qR&#10;JVI2sOt6UJgd+ZiDgffL8ekFlFSbvV0oo4EbChzGx4f9GRdb25HMsYhqlCwG5lrLq9biZkxWOiqY&#10;22YiTra2kYMu1l1tQD30/bPm3wwYN0x18gb45AdQl1tp5j/sFB2T0FQ7R0nTNEV3j6o9feQzro1i&#10;OWA14Fm+Q8a1a8+Bvu/d/dMb2JY5uiPbhG/ktn4cqGU/er3pcvwCAAD//wMAUEsDBBQABgAIAAAA&#10;IQDQg5pQVgIAALEEAAAOAAAAZHJzL2Uyb0RvYy54bWysVE1v2zAMvQ/YfxB0X+2k+WiDOEXWosOA&#10;oi3QDj0rstwYk0VNUmJ3v35PipMl3U7DLgJFPj+Rj6TnV12j2VY5X5Mp+OAs50wZSWVtXgv+7fn2&#10;0wVnPghTCk1GFfxNeX61+Phh3tqZGtKadKkcA4nxs9YWfB2CnWWZl2vVCH9GVhkEK3KNCLi616x0&#10;ogV7o7Nhnk+yllxpHUnlPbw3uyBfJP6qUjI8VJVXgemCI7eQTpfOVTyzxVzMXp2w61r2aYh/yKIR&#10;tcGjB6obEQTbuPoPqqaWjjxV4UxSk1FV1VKlGlDNIH9XzdNaWJVqgTjeHmTy/49W3m8fHatL9I4z&#10;Ixq06Fl1gX2mjg2iOq31M4CeLGChgzsie7+HMxbdVa5hjiDu4HI8ml5MpkkLVMcAh+xvB6kjt4Tz&#10;fDi8uJyOOZOIwZ7keWpGtmOLrNb58EVRw6JRcIdeJlqxvfMBGQC6h0S4J12Xt7XW6RLnR11rx7YC&#10;ndch5YwvTlDasLbgk/NxnohPYpH68P1KC/k9Vn3KgJs2cEaNdlpEK3SrrhdoReUbdEvSQAZv5W0N&#10;3jvhw6NwGDQ4sTzhAUelCclQb3G2Jvfzb/6IR/8R5azF4Bbc/9gIpzjTXw0m43IwGsVJT5fReDrE&#10;xR1HVscRs2muCQqh+8gumREf9N6sHDUv2LFlfBUhYSTeLnjYm9dht07YUamWywTCbFsR7syTlZF6&#10;383n7kU42/czYBTuaT/iYvaurTts/NLQchOoqlPPo8A7VXvdsRepLf0Ox8U7vifU7z/N4hcAAAD/&#10;/wMAUEsDBBQABgAIAAAAIQBh17L63wAAAAoBAAAPAAAAZHJzL2Rvd25yZXYueG1sTI9BT4NAEIXv&#10;Jv6HzZh4s0ubgpayNIboSW3Syg9Y2BGI7CyyS0v99Y4nPU3ezMub72W72fbihKPvHClYLiIQSLUz&#10;HTUKyvfnuwcQPmgyuneECi7oYZdfX2U6Ne5MBzwdQyM4hHyqFbQhDKmUvm7Rar9wAxLfPtxodWA5&#10;NtKM+szhtperKEqk1R3xh1YPWLRYfx4nq8APVfz9VQxPb+WUNC+vZbGPDhelbm/mxy2IgHP4M8Mv&#10;PqNDzkyVm8h40bNer2K2Ktjc82RDEi+5XKVgHfNG5pn8XyH/AQAA//8DAFBLAQItABQABgAIAAAA&#10;IQC2gziS/gAAAOEBAAATAAAAAAAAAAAAAAAAAAAAAABbQ29udGVudF9UeXBlc10ueG1sUEsBAi0A&#10;FAAGAAgAAAAhADj9If/WAAAAlAEAAAsAAAAAAAAAAAAAAAAALwEAAF9yZWxzLy5yZWxzUEsBAi0A&#10;FAAGAAgAAAAhANCDmlBWAgAAsQQAAA4AAAAAAAAAAAAAAAAALgIAAGRycy9lMm9Eb2MueG1sUEsB&#10;Ai0AFAAGAAgAAAAhAGHXsvrfAAAACgEAAA8AAAAAAAAAAAAAAAAAsAQAAGRycy9kb3ducmV2Lnht&#10;bFBLBQYAAAAABAAEAPMAAAC8BQAAAAA=&#10;\" fillcolor=\"white [3201]\" strokeweight=\".5pt\">\n", text_box_id);
    extract_docx_char_append_string(content, "            <v:textbox>\n");
    extract_docx_char_append_string(content, "              <w:txbxContent>");

    for (int p=paragraph_begin; p<paragraph_end; ++p) {
        paragraph_t* paragraph = page->paragraphs[p];
        if (extract_document_to_docx_content_paragraph(state, paragraph, content)) goto end;
    }

    extract_docx_char_append_string(content, "\n");
    extract_docx_char_append_string(content, "\n");
    extract_docx_char_append_string(content, "              </w:txbxContent>\n");
    extract_docx_char_append_string(content, "            </v:textbox>\n");
    extract_docx_char_append_string(content, "          </v:shape>\n");
    extract_docx_char_append_string(content, "        </w:pict>\n");
    extract_docx_char_append_string(content, "      </mc:Fallback>\n");
    extract_docx_char_append_string(content, "    </mc:AlternateContent>\n");
    extract_docx_char_append_string(content, "  </w:r>\n");
    extract_docx_char_append_string(content, "</w:p>");
    e = 0;
    end:
    return e;
}


int extract_document_to_docx_content(
        extract_document_t* document,
        int                 spacing,
        int                 rotation,
        int                 images,
        char**              o_content,
        size_t*             o_content_length
        )
{
    int ret = -1;
    int text_box_id = 0;
    int p;

    extract_astring_t   content;
    extract_astring_init(&content);
    
    /* Write paragraphs into <content>. */
    for (p=0; p<document->pages_num; ++p) {
        page_t* page = document->pages[p];
        int p;
        content_state_t state;
        state.font_name = NULL;
        state.font_size = 0;
        state.font_bold = 0;
        state.font_italic = 0;
        state.ctm_prev = NULL;
        
        for (p=0; p<page->paragraphs_num; ++p) {
            paragraph_t* paragraph = page->paragraphs[p];
            const matrix_t* ctm = &paragraph->lines[0]->spans[0]->ctm;
            float rotate = atan2f(ctm->b, ctm->a);
            
            if (spacing
                    && state.ctm_prev
                    && paragraph->lines_num
                    && paragraph->lines[0]->spans_num
                    && matrix_cmp4(
                            state.ctm_prev,
                            &paragraph->lines[0]->spans[0]->ctm
                            )
                    ) {
                /* Extra vertical space between paragraphs that were at
                different angles in the original document. */
                if (extract_docx_paragraph_empty(&content)) goto end;
            }

            if (spacing) {
                /* Extra vertical space between paragraphs. */
                if (extract_docx_paragraph_empty(&content)) goto end;
            }
            
            if (rotation && rotate != 0) {
            
                /* Find extent of paragraphs with this same rotation. extent
                will contain max width and max height of paragraphs, in units
                before application of ctm, i.e. before rotation. */
                point_t extent = {0, 0};
                int p0 = p;
                int p1;
                
                outf("rotate=%.2frad=%.1fdeg ctm: ef=(%f %f) abcd=(%f %f %f %f)",
                        rotate, rotate * 180 / g_pi,
                        ctm->e,
                        ctm->f,
                        ctm->a,
                        ctm->b,
                        ctm->c,
                        ctm->d
                        );
                
                {
                    /* We assume that first span is at origin of text
                    block. This assumes left-to-right text. */
                    float rotate0 = rotate;
                    const matrix_t* ctm0 = ctm;
                    point_t origin = {
                            paragraph->lines[0]->spans[0]->chars[0].x,
                            paragraph->lines[0]->spans[0]->chars[0].y
                            };
                    matrix_t ctm_inverse = {1, 0, 0, 1, 0, 0};
                    float ctm_det = ctm->a*ctm->d - ctm->b*ctm->c;
                    if (ctm_det != 0) {
                        ctm_inverse.a = +ctm->d / ctm_det;
                        ctm_inverse.b = -ctm->b / ctm_det;
                        ctm_inverse.c = -ctm->c / ctm_det;
                        ctm_inverse.d = +ctm->a / ctm_det;
                    }
                    else {
                        outf("cannot invert ctm=(%f %f %f %f)",
                                ctm->a, ctm->b, ctm->c, ctm->d);
                    }

                    for (p=p0; p<page->paragraphs_num; ++p) {
                        paragraph = page->paragraphs[p];
                        ctm = &paragraph->lines[0]->spans[0]->ctm;
                        rotate = atan2f(ctm->b, ctm->a);
                        if (rotate != rotate0) {
                            break;
                        }

                        /* Update <extent>. */
                        {
                            int l;
                            for (l=0; l<paragraph->lines_num; ++l) {
                                line_t* line = paragraph->lines[l];
                                span_t* span = line_span_last(line);
                                char_t* char_ = span_char_last(span);
                                float adv = char_->adv * matrix_expansion(span->trm);
                                float x = char_->x + adv * cosf(rotate);
                                float y = char_->y + adv * sinf(rotate);

                                float dx = x - origin.x;
                                float dy = y - origin.y;

                                /* Position relative to origin and before box rotation. */
                                float xx = ctm_inverse.a * dx + ctm_inverse.b * dy;
                                float yy = ctm_inverse.c * dx + ctm_inverse.d * dy;
                                yy = -yy;
                                if (xx > extent.x) extent.x = xx;
                                if (yy > extent.y) extent.y = yy;
                                if (0) outf("rotate=%f p=%i: origin=(%f %f) xy=(%f %f) dxy=(%f %f) xxyy=(%f %f) span: %s",
                                        rotate, p, origin.x, origin.y, x, y, dx, dy, xx, yy, span_string(span));
                            }
                        }
                    }
                    p1 = p;
                    rotate = rotate0;
                    ctm = ctm0;
                    outf("rotate=%f p0=%i p1=%i. extent is: (%f %f)",
                            rotate, p0, p1, extent.x, extent.y);
                }
                
                /* Paragraphs p0..p1-1 have same rotation. We output them into
                a single rotated text box. */
                
                /* We need unique id for text box. */
                text_box_id += 1;
                
                {
                    /* Angles are in units of 1/60,000 degree. */
                    int rot = (int) (rotate * 180 / g_pi * 60000);

                    /* <wp:anchor distT=\.. etc are in EMU - 1/360,000 of a cm.
                    relativeHeight is z-ordering. (wp:positionV:wp:posOffset,
                    wp:positionV:wp:posOffset) is position of origin of box in
                    EMU.

                    The box rotates about its centre but we want to rotate
                    about the origin (top-left). So we correct the position of
                    box by subtracting the vector that the top-left moves when
                    rotated by angle <rotate> about the middle. */
                    float point_to_emu = 12700;   /* https://en.wikipedia.org/wiki/Office_Open_XML_file_formats#DrawingML */
                    int x = (int) (ctm->e * point_to_emu);
                    int y = (int) (ctm->f * point_to_emu);
                    int w = (int) (extent.x * point_to_emu);
                    int h = (int) (extent.y * point_to_emu);
                    int dx;
                    int dy;

                    if (0) outf("rotate: %f rad, %f deg. rot=%i", rotate, rotate*180/g_pi, rot);

                    h *= 2;
                    /* We can't predict how much space Word will actually
                    require for the rotated text, so make the box have the
                    original width but allow text to take extra vertical
                    space. There doesn't seem to be a way to make the text box
                    auto-grow to contain the text. */

                    dx = (int) (w/2 * (1-cos(rotate)) + h/2 * sin(rotate));
                    dy = (int) (h/2 * (cos(rotate)-1) + w/2 * sin(rotate));
                    outf("ctm->e,f=%f,%f rotate=%f => x,y=%ik %ik dx,dy=%ik %ik",
                            ctm->e,
                            ctm->f,
                            rotate * 180/g_pi,
                            x/1000,
                            y/1000,
                            dx/1000,
                            dy/1000
                            );
                    x -= dx;
                    y -= -dy;

                    if (extract_document_output_rotated_paragraphs(page, p0, p1, rot, x, y, w, h, text_box_id, &content, &state)) goto end;
                }
                p = p1 - 1;
                //p = page->paragraphs_num - 1;
            }
            else {
                if (extract_document_to_docx_content_paragraph(&state, paragraph, &content)) goto end;
            }
        
        }
        
        if (images) {
            int i;
            for (i=0; i<page->images_num; ++i) {
                extract_document_append_image(&content, &page->images[i]);
            }
        }
    }
    ret = 0;

    end:

    if (ret) {
        extract_astring_free(&content);
        *o_content = NULL;
        *o_content_length = 0;
    }
    else {
        *o_content = content.chars;
        *o_content_length = content.chars_num;
        content.chars = NULL;
        content.chars_num = 0;
    }

    return ret;
}

int extract_document_join(extract_document_t* document)
{
    int ret = -1;

    /* Now for each page we join spans into lines and paragraphs. A line is a
    list of spans that are at the same angle and on the same line. A paragraph
    is a list of lines that are at the same angle and close together. */
    int p;
    for (p=0; p<document->pages_num; ++p) {
        page_t* page = document->pages[p];
        outf("processing page %i: num_spans=%i", p, page->spans_num);

        if (make_lines(
                page->spans,
                page->spans_num,
                &page->lines,
                &page->lines_num
                )) goto end;

        if (make_paragraphs(
                page->lines,
                page->lines_num,
                &page->paragraphs,
                &page->paragraphs_num
                )) goto end;
    }

    ret = 0;

    end:

    return ret;
}

void extract_end(void)
{
    span_string(NULL);
}
