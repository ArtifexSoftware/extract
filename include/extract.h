#ifndef ARITFEX_EXTRACT_H
#define ARITFEX_EXTRACT_H

#include "extract_buffer.h"


/* Functions for extracting paragraphs of text from intermediate format data
created by these commands:

    mutool draw -F xmltext ...
    gs -sDEVICE=txtwrite -dTextFormat=4 ... 

Unless otherwise stated, all functions return 0 on success or -1 with errno
set.
*/


typedef struct extract_document_t extract_document_t;
/* Contains characters, spans, lines, paragraphs and pages. */

void extract_document_free(extract_document_t** io_document);
/* Frees a document. Does nothing if *io_document is NULL. Always sets
*io_document to NULL.
*/


int extract_intermediate_to_document(
        extract_buffer_t*       buffer,
        int                     autosplit,
        extract_document_t**    o_document
        );
/* Reads intermediate format from buffer, into a document.

buffer;
    Source of intermediate format data.
autosplit:
    If true, we split spans when the y coordinate changes, in order to stress
    the joining algorithms.
o_document:
    Out-param: *o_document is set to internal data populated with pages from
    intermediate format. Each page will have spans, but no lines or paragraphs;
    use extract_document_join() to create lines and paragraphs.

    *o_document should be freed with extract_document_free().

Returns with *o_document set. On error *o_document=NULL.
*/


int extract_document_join(extract_document_t* document);
/* Finds lines and paragraphs in document.

document:
    Should have spans, but no lines or paragraphs, e.g. from
    extract_intermediate_to_document().
    
Returns with document containing lines and paragraphs.
*/


int extract_document_to_docx_content(
        extract_document_t* document,
        int                 spacing,
        int                 rotation,
        int                 images,
        char**              o_content,
        size_t*             o_content_length
        );
/* Reads from document and converts into docx content for embedding inside the
word/document.xml item within the .docx.

document:
    Should contain paragraphs e.g. from extract_document_join().
spacing:
    If non-zero, we add extra vertical space between paragraphs.
rotation:
    If non-zero we output rotated text inside a rotated drawing. Otherwise
    output text is always horizontal.
images:
    If non-zero we include images.
o_content:
    Out param: set to point to zero-terminated text in buffer from malloc().
o_content_length:
    Out-param: set to length of returned string.

On error *o_content=NULL and *o_content_length=0.
*/


int extract_docx_content_to_docx(
        const char*             content,
        size_t                  content_length,
        extract_document_t*     document,
        extract_buffer_t*       buffer
        );
/* Writes a .docx file to an extract_buffer_t.

Uses internal template docx.

content:
    E.g. from extract_document_to_docx_content().
content_length:
    Length of content.
document:
    Information about any images in <document> will be included in the
    generated .docx file. <content> should contain references to these
    images.
buffer:
    Where to write the zipped docx contents.
*/


int extract_docx_content_to_docx_template(
        const char*         content,
        size_t              content_length,
        extract_document_t* document,
        const char*         path_template,
        const char*         path_out,
        int                 preserve_dir
        );
/* Creates a new .docx file using a provided template document.

Uses the 'zip' and 'unzip' commands internally.

content:
    E.g. from extract_document_to_docx_content().
content_length:
    Length of content.
path_template:
    Name of .docx file to use as a template.
preserve_dir:
    If true, we don't delete the temporary directory <path_out>.dir containing
    unzipped .docx content.
path_out:
    Name of .docx file to create. Must not contain single-quote, double quote,
    space or ".." sequence - these will force EINVAL error because they could
    make internal shell commands unsafe.
*/

#endif
