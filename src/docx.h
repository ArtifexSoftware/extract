#ifndef ARTIFEX_EXTRACT_DOCX_H
#define ARTIFEX_EXTRACT_DOCX_H

/* Only for internal use by extract code.  */

/* Things for creating .docx files. */

int extract_docx_run_start(
        extract_astring_t*  content,
        const char*         font_name,
        double              font_size,
        int                 bold,
        int                 italic
        );
/* Starts a new run. Caller must ensure that extract_docx_run_finish() was
called to terminate any previous run. */

int extract_docx_run_finish(extract_astring_t* content);

int extract_docx_paragraph_start(extract_astring_t* content);

int extract_docx_paragraph_finish(extract_astring_t* content);

int extract_docx_char_append_string(extract_astring_t* content, char* text);
int extract_docx_char_append_stringf(extract_astring_t* content, char* format, ...);

int extract_docx_char_append_char(extract_astring_t* content, char c);

int extract_docx_char_truncate_if(extract_astring_t* content, char c);
/* Removes last char if it is <c>. */

int extract_docx_paragraph_empty(extract_astring_t* content);
/* Append an empty paragraph. */


#endif
