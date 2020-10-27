/* Command-line programme for extract_ API. */

#include "../include/extract.h"

#include "alloc.h"
#include "memento.h"
#include "outf.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* Error-detecting equivalent to *out = argv[++i].
*/
static int arg_next_string(char** argv, int argc, int* i, const char** out)
{
    if (*i + 1 >= argc) {
        printf("Expected arg after: %s\n", argv[*i]);
        errno = EINVAL;
        return -1;
    }
    *i += 1;
    *out = argv[*i];
    return 0;
}

/* Error-detecting equivalent to *out = atoi(argv[++i]).
*/
static int arg_next_int(char** argv, int argc, int* i, int* out)
{
    if (*i + 1 >= argc) {
        printf("Expected integer arg after: %s\n", argv[*i]);
        errno = EINVAL;
        return -1;
    }
    *i += 1;
    *out = atoi(argv[*i]);
    return 0;
}


int main(int argc, char** argv)
{
    const char* docx_out_path       = NULL;
    const char* input_path          = NULL;
    const char* docx_template_path  = NULL;
    const char* content_path        = NULL;
    int         preserve_dir        = 0;
    int         spacing             = 1;
    int         rotation            = 1;
    int         autosplit           = 0;
    int         images              = 1;
    int         alloc_stats         = 0;

    extract_document_t* document = NULL;
    char*               content = NULL;
    size_t              content_length = 0;
    extract_buffer_t*   intermediate = NULL;
    extract_buffer_t*   out_buffer = NULL;

    int e = -1;
    
    for (int i=1; i<argc; ++i) {
        const char* arg = argv[i];
        if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
            printf(
                    "Converts intermediate data from mupdf or gs into a .docx file.\n"
                    "\n"
                    "We require a file containing XML output from one of these commands:\n"
                    "    mutool draw -F xmltext ...\n"
                    "    gs -sDEVICE=txtwrite -dTextFormat=4 ...\n"
                    "\n"
                    "We also requires a template .docx file.\n"
                    "\n"
                    "Args:\n"
                    "    --autosplit 0|1\n"
                    "        If 1, we initially split spans when y coordinate changes. This\n"
                    "        stresses our handling of spans when input is from mupdf.\n"
                    "    -i <intermediate-path>\n"
                    "        Path of XML file containing intermediate text spans.\n"
                    "    -o <docx-path>\n"
                    "        If specified, we generate the specified .docx file.\n"
                    "    --o-content <path>\n"
                    "        If specified, we write raw .docx content to <path>; this is the\n"
                    "        text that we embed inside the template word/document.xml file\n"
                    "        when generating the .docx file.\n"
                    "    -p 0|1\n"
                    "        If 1 and -t <docx-template> is specified, we preserve the\n"
                    "        uncompressed <docx-path>.lib/ directory.\n"
                    "    -r 0|1\n"
                    "       If 1, we we output rotated text inside a rotated drawing. Otherwise\n"
                    "       output text is always horizontal.\n"
                    "    -s 0|1\n"
                    "        If 1, we insert extra vertical space between paragraphs and extra\n"
                    "        vertical space between paragraphs that had different ctm matrices\n"
                    "        in the original document.\n"
                    "    -t <docx-template>\n"
                    "        If specified we use <docx-template> as template. Otheerwise we use"
                    "        an internal template.\n"
                    "    -v <verbose>\n"
                    "        Set verbose level.\n"
                    "    -v-alloc\n"
                    "        Show alloc stats.\n"
                    );
            if (i + 1 == argc) {
                e = 0;
                goto end;
            }
        }
        else if (!strcmp(arg, "--alloc-exp-min")) {
            int size;
            if (arg_next_int(argv, argc, &i, &size)) goto end;
            outf0("Calling alloc_set_min_alloc_size(%i)", size);
            extract_alloc_exp_min(size);
        }
        else if (!strcmp(arg, "--autosplit")) {
            if (arg_next_int(argv, argc, &i, &autosplit)) goto end;
        }
        else if (!strcmp(arg, "-i")) {
            if (arg_next_string(argv, argc, &i, &input_path)) goto end;
        }
        else if (!strcmp(arg, "-o")) {
            if (arg_next_string(argv, argc, &i, &docx_out_path)) goto end;
        }
        else if (!strcmp(arg, "--o-content")) {
            if (arg_next_string(argv, argc, &i, &content_path)) goto end;
        }
        else if (!strcmp(arg, "-p")) {
            if (arg_next_int(argv, argc, &i, &preserve_dir)) goto end;
        }
        else if (!strcmp(arg, "-r")) {
            if (arg_next_int(argv, argc, &i, &rotation)) goto end;
        }
        else if (!strcmp(arg, "-s")) {
            if (arg_next_int(argv, argc, &i, &spacing)) goto end;
        }
        else if (!strcmp(arg, "-t")) {
            if (arg_next_string(argv, argc, &i, &docx_template_path)) goto end;
        }
        else if (!strcmp(arg, "-v")) {
            int verbose;
            if (arg_next_int(argv, argc, &i, &verbose)) goto end;
            outf_verbose_set(verbose);
            outf("Have changed verbose to %i", verbose);
        }
        else if (!strcmp(arg, "--v-alloc")) {
            if (arg_next_int(argv, argc, &i, &alloc_stats)) goto end;
        }
        else {
            printf("Unrecognised arg: '%s'\n", arg);
            errno = EINVAL;
            goto end;
        }

        assert(i < argc);
    }

    if (!input_path) {
        printf("-i <input-path> not specified.\n");
        errno = EINVAL;
        goto end;
    }
    
    if (extract_buffer_open_file(input_path, 0 /*writable*/, &intermediate)) {
        printf("Failed to open intermediate file: %s\n", input_path);
        goto end;
    }
    
    if (!content_path && docx_out_path && !docx_template_path) {
        outf("calling extract_intermediate_to_docx()");
        if (extract_buffer_open_file(docx_out_path, 1 /*writable*/, &out_buffer)) goto end;
        if (extract_intermediate_to_docx(
                intermediate,
                autosplit,
                spacing,
                rotation,
                images,
                out_buffer
                )) {
            goto end;
        }
    }
    else {
        if (extract_intermediate_to_document(intermediate, autosplit, &document)) {
            printf("Failed to read 'raw' output from: %s\n", input_path);
            goto end;
        }

        if (extract_document_join(document)) {
            printf("Failed to join spans into lines and paragraphs.\n");
            goto end;
        }

        if (extract_document_to_docx_content(document, spacing, rotation, images, &content, &content_length)) {
            printf("Failed to create docx content.\n");
            goto end;
        }

        if (content_path) {
            FILE* f;
            printf("Writing content to: %s\n", content_path);
            f = fopen(content_path, "w");
            if (!f) {
                printf("Failed to create content file: %s\n", content_path);
                goto end;
            }
            if (fwrite(content, content_length, 1 /*nmemb*/, f) != 1) {
                printf("Failed to write to content file: %s\n", content_path);
                fclose(f);
                errno = EIO;
                goto end;
            }
            fclose(f);
        }

        if (docx_out_path) {
            printf("Creating .docx file: %s\n", docx_out_path);
            if (docx_template_path) {
                printf("Using template: %s\n", docx_template_path);
                if (extract_docx_content_to_docx_template(
                        content,
                        content_length,
                        document,
                        docx_template_path,
                        docx_out_path,
                        preserve_dir
                        )) {
                    printf("Failed to create .docx file: %s\n", docx_out_path);
                    goto end;
                }
            }
            else {
                /* We could use extract_docx_content_to_docx(), but
                instead test with extract_buffer_open_file() and
                extract_docx_content_to_docx_buffer(). */
                if (extract_buffer_open_file(docx_out_path, 1 /*writable*/, &out_buffer)) goto end;

                if (extract_docx_content_to_docx(
                        content,
                        content_length,
                        document,
                        out_buffer
                        )) {
                    printf("Failed to create .docx file: %s\n", docx_out_path);
                    goto end;
                }
                if (extract_buffer_close(&out_buffer)) goto end;
            }
        }
    }

    e = 0;
    end:

    extract_free(&content);
    extract_document_free(&document);
    extract_buffer_close(&intermediate);
    extract_buffer_close(&out_buffer);

    if (e) {
        printf("Failed (errno=%i): %s\n", errno, strerror(errno));
        return 1;
    }
    
    extract_end();
    
    if (alloc_stats) {
        printf("Alloc stats: num_malloc=%i num_realloc=%i num_free=%i num_libc_realloc=%i\n",
                extract_alloc_info.num_malloc,
                extract_alloc_info.num_realloc,
                extract_alloc_info.num_free,
                extract_alloc_info.num_libc_realloc
                );
    }
    printf("Finished.\n");
    return 0;
}
