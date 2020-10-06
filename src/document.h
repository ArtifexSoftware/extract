#ifndef ARTIFEX_EXTRACT_DOCUMENT_H
#define ARTIFEX_EXTRACT_DOCUMENT_H

/* extract.c defines the document_t struct and uses it internally, but docx.c
needs to extract images from a document_t in order to generate a .docx file.

Instead of making document_t visible outside of extract.c, we instead define
a limited API here to extract image information. The code is implemented in
extract.c. */

typedef struct
{
    const char* name;
    const char* id;
    const char* data;
    size_t      data_size;
} extract_document_image_t;

typedef struct
{
    extract_document_image_t*   images;
    int                         images_num;
    char**                      imagetypes;
    int                         imagetypes_num;
} extract_document_imagesinfo_t;

int extract_document_imageinfos(extract_document_t* document, extract_document_imagesinfo_t* imageinfos);
/* Returns information on images in <document>.

On return, <imageinfos> will contain pointers into <document>.
*/

void extract_document_imageinfos_free(extract_document_imagesinfo_t* o_imageinfos);

#endif
