#include "document.h"
#include "outf.h"
#include <assert.h>

void
content_init(content_t *content, content_type_t type)
{
    content->type = type;
    content->next = content->prev = (type == content_root) ? content : NULL;
}

void
content_unlink(content_t *content)
{
    if (content == NULL)
        return;
    assert(content->type != content_root);
    if (content->prev == NULL) {
        assert(content->next == NULL);
        /* Already unlinked */
    } else {
        assert(content->next != content && content->prev != content);
        content->prev->next = content->next;
        content->next->prev = content->prev;
        content->next = content->prev = NULL;
    }
}

void content_unlink_span(span_t *span)
{
    content_unlink(&span->base);
}

void extract_span_init(span_t *span)
{
    static const span_t blank = { 0 };
    *span = blank;
    content_init(&span->base, content_span);
}

void extract_span_free(extract_alloc_t* alloc, span_t **pspan)
{
    if (!*pspan) return;
    content_unlink(&(*pspan)->base);
    extract_free(alloc, &(*pspan)->font_name);
    extract_free(alloc, &(*pspan)->chars);
    extract_free(alloc, pspan);
}

void extract_image_init(image_t *image)
{
    static const image_t blank = { 0 };
    *image = blank;
    content_init(&image->base, content_image);
}

void
content_clear(extract_alloc_t* alloc, content_t *proot)
{
    content_t *content, *next;

    assert(proot->type == content_root && proot->next != NULL && proot->prev != NULL);
    for (content = proot->next; content != proot; content = next)
    {
        assert(content->type != content_root);
        next = content->next;
        switch (content->type)
        {
            default:
            case content_root:
                assert("This never happens" == NULL);
                break;
            case content_span:
                extract_span_free(alloc, (span_t **)&content);
                break;
            case content_image:
                extract_image_free(alloc, (image_t **)&content);
                break;
        }
    }
}

int content_count_spans(content_t *root)
{
    int n = 0;
    content_t *s;

    for (s = root->next; s != root; s = s->next)
        if (s->type == content_span) n++;

    return n;
}

int content_count_images(content_t *root)
{
    int n = 0;
    content_t *s;

    for (s = root->next; s != root; s = s->next)
        if (s->type == content_image) n++;

    return n;
}

span_t *content_first_span(content_t *root)
{
    content_t *span;
    assert(root && root->type == content_root);

    for (span = root->next; span != root; span = span->next)
    {
        if (span->type == content_span)
            return (span_t *)span;
    }
    return NULL;
}

span_t *content_last_span(content_t *root)
{
    content_t *span;
    assert(root && root->type == content_root);

    for (span = root->prev; span != root; span = span->prev)
    {
        if (span->type == content_span)
            return (span_t *)span;
    }
    return NULL;
}

void extract_line_free(extract_alloc_t* alloc, line_t** pline)
{
    line_t* line = *pline;
    content_clear(alloc, &line->content);
    extract_free(alloc, pline);
}

void extract_lines_free(extract_alloc_t* alloc, line_t*** plines, int lines_num)
{
    int l;
    line_t** lines = *plines;
    for (l=0; l<lines_num; ++l)
    {
        extract_line_free(alloc, &lines[l]);
    }
    extract_free(alloc, plines);
}

void extract_image_clear(extract_alloc_t* alloc, image_t* image)
{
    extract_free(alloc, &image->type);
    extract_free(alloc, &image->name);
    extract_free(alloc, &image->id);
    if (image->data_free) {
        image->data_free(image->data_free_handle, image->data);
    }
}

void extract_image_free(extract_alloc_t *alloc, image_t **pimage)
{
    if (*pimage == NULL)
        return;
    extract_image_clear(alloc, *pimage);
    extract_free(alloc, pimage);
}

void extract_cell_free(extract_alloc_t* alloc, cell_t** pcell)
{
    int p;
    cell_t* cell = *pcell;
    if (!cell) return;

    outf("cell->lines_num=%i", cell->lines_num);
    outf("cell->paragraphs_num=%i", cell->paragraphs_num);
    extract_lines_free(alloc, &cell->lines, cell->lines_num);

    outf("cell=%p cell->paragraphs_num=%i", cell, cell->paragraphs_num);
    for (p=0; p<cell->paragraphs_num; ++p)
    {
        paragraph_t* paragraph = cell->paragraphs[p];
        outf("paragraph->lines_num=%i", paragraph->lines_num);
        /* We don't attempt to free paragraph->lines[] because they point into
        cell->lines which are already freed. */
        extract_free(alloc, &paragraph->lines);
        extract_free(alloc, &cell->paragraphs[p]);
    }
    extract_free(alloc, &cell->paragraphs);
    extract_free(alloc, pcell);
}

int
extract_split_alloc(extract_alloc_t* alloc, split_type_t type, int count, split_t** psplit)
{
    split_t *split;

    if (extract_malloc(alloc, psplit, sizeof(*split) + (count-1) * sizeof(split_t *)))
    {
        return -1;
    }

    split = *psplit;
    split->type = type;
    split->weight = 0;
    split->count = count;
    memset(&split->split[0], 0, sizeof(split_t *) * count);

    return 0;
}

void extract_split_free(extract_alloc_t *alloc, split_t **psplit)
{
    int i;
    split_t *split = *psplit;

    if (!split)
        return;

    for (i = 0; i < split->count; i++)
        extract_split_free(alloc, &split->split[i]);
    extract_free(alloc, psplit);
}
