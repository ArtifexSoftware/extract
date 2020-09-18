#ifndef EXTRACT_DOCX_TEMPLATE_H
#define EXTRACT_DOCX_TEMPLATE_H

/* THIS IS AUTO-GENERATED CODE, DO NOT EDIT. */

#include "zip.h"

extern char extract_docx_word_document_xml[];
extern int  extract_docx_word_document_xml_length;
/* Contents of internal .docx template's word/document.xml. */

int extract_docx_write(extract_zip_t* zip, const char* word_document_xml, int word_document_xml_length);
/* Writes internal template .docx items to <zip>, using <word_document_xml>
instead of the internal template's word/document.xml. */

#endif
