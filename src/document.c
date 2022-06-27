#include "document.h"
#include "outf.h"
#include <assert.h>
#include <stdio.h>

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

void extract_line_init(line_t *line)
{
    static const line_t blank = { 0 };
    *line = blank;
    content_init(&line->base, content_line);
    content_init(&line->content, content_root);
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
            case content_line:
                extract_line_free(alloc, (line_t **)&content);
                break;
            case content_image:
                extract_image_free(alloc, (image_t **)&content);
                break;
        }
    }
}

static int
content_count_type(content_t *root, content_type_t type)
{
    int n = 0;
    content_t *s;

    for (s = root->next; s != root; s = s->next)
        if (s->type == type) n++;

    return n;
}

int content_count_spans(content_t *root)
{
    return content_count_type(root, content_span);
}

int content_count_images(content_t *root)
{
    return content_count_type(root, content_image);
}

int content_count_lines(content_t *root)
{
    return content_count_type(root, content_line);
}

static content_t *
content_first_of_type(const content_t *root, content_type_t type)
{
    content_t *content;
    assert(root && root->type == content_root);

    for (content = root->next; content != root; content = content->next)
    {
        if (content->type == type)
            return content;
    }
    return NULL;
}

static content_t *
content_last_of_type(const content_t *root, content_type_t type)
{
    content_t *content;
    assert(root && root->type == content_root);

    for (content = root->prev; content != root; content = content->prev)
    {
        if (content->type == type)
            return content;
    }
    return NULL;
}

span_t *content_first_span(const content_t *root)
{
    return (span_t *)content_first_of_type(root, content_span);
}

span_t *content_last_span(const content_t *root)
{
    return (span_t *)content_last_of_type(root, content_span);
}

line_t *content_first_line(const content_t *root)
{
    return (line_t *)content_first_of_type(root, content_line);
}

line_t *content_last_line(const content_t *root)
{
    return (line_t *)content_last_of_type(root, content_line);
}

void
content_concat(content_t *dst, content_t *src)
{
    content_t *walk, *walk_next;
    assert(dst->type == content_root);
    if (src == NULL)
        return;
    assert(src->type == content_root);

    for (walk = src->next; walk != src; walk = walk_next)
    {
        walk_next = walk->next;
        content_append(dst, walk);
    }
}


void extract_line_free(extract_alloc_t* alloc, line_t** pline)
{
    line_t* line = *pline;
    content_unlink(&(*pline)->base);
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

    outf("cell->lines_num=%i", content_count_lines(&cell->lines));
    outf("cell->paragraphs_num=%i", cell->paragraphs_num);
    content_clear(alloc, &cell->lines);

    outf("cell=%p cell->paragraphs_num=%i", cell, cell->paragraphs_num);
    for (p=0; p<cell->paragraphs_num; ++p)
    {
        paragraph_t* paragraph = cell->paragraphs[p];
        outf("paragraph->lines_num=%i", content_count_lines(&paragraph->content));
        /* We don't attempt to free paragraph->lines[] because they point into
        cell->lines which are already freed. */
        content_clear(alloc, &paragraph->content);
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

static void space_prefix(int depth)
{
    while (depth-- > 0)
    {
        putc(' ', stdout);
    }
}

static void dump_span(const span_t *span, int depth)
{
    int i;
    space_prefix(depth);
    printf("chars=\"");
    for (i = 0; i < span->chars_num; i++)
    {
        if (span->chars[i].ucs >= 32 && span->chars[i].ucs <= 127)
        {
             putc((char)span->chars[i].ucs, stdout);
        }
        else
        {
             printf("<%04x>", span->chars[i].ucs);
        }
    }
    printf("\">\n");
}

static void
content_dump_aux(const content_t *content, int depth)
{
    const content_t *walk;

    assert(content->type == content_root);
    for (walk = content->next; walk != content; walk = walk->next)
    {
        space_prefix(depth);
        switch (walk->type)
        {
            case content_span:
                printf("<span>\n");
                dump_span((const span_t *)walk, depth+1);
                printf("</span>\n");
                break;
            case content_line:
                printf("<line>\n");
                content_dump_aux(&((const line_t *)walk)->content, depth+1);
                space_prefix(depth);
                printf("</line>\n");
                break;
            case content_image:
                printf("<image/>\n");
                break;
            default:
                assert("Unexpected type found while dumping content list." == NULL);
                break;
        }
    }
}

void content_dump(const content_t *content)
{
    content_dump_aux(content, 0);
}
