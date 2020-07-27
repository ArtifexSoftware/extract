#include "outf.h"
#include "zip.h"

#include <zlib.h>
/* For crc32(). */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


typedef struct
{
    int16_t     mtime;
    int16_t     mdate;
    int32_t     crc_sum;
    int32_t     size_compressed;
    int32_t     size_uncompressed;
    char*       name;
    uint32_t    offset;
    
} extract_zip_cd_file_t;

struct extract_zip_t
{
    extract_buffer_t*       buffer;
    extract_zip_cd_file_t*  cd_files;
    int                     cd_files_num;
    int                     errno_;
    int                     eof;
    
};

int extract_zip_open(extract_buffer_t* buffer, extract_zip_t** o_zip)
{
    int e = -1;
    extract_zip_t* zip = malloc(sizeof(*zip));
    if (!zip)   goto end;
    
    zip->cd_files = NULL;
    zip->cd_files_num = 0;
    zip->buffer = buffer;
    zip->errno_ = 0;
    zip->eof = 0;
    e = 0;
    
    end:
    if (e) {
        *o_zip = NULL;
    }
    else {
        *o_zip = zip;
    }
    return e;
}

static int s_native_little_endinesss(void)
{
    static const char   a[] = { 1, 2};
    uint16_t b = *(uint16_t*) a;
    if (b == 1 + 2*256) {
        /* Native little-endiness. */
        return 1;
    }
    else if (b == 2 + 1*256) {
        return 0;
    }
    assert(0);
}

static int s_write(extract_zip_t* zip, const void* data, size_t data_length)
{
    if (zip->errno_)    return -1;
    if (zip->eof)       return +1;
    size_t actual;
    int e = extract_buffer_write(zip->buffer, data, data_length, &actual);
    if (e == -1)    zip->errno_ = errno;
    if (e == +1)    zip->eof = 1;
    return e;
}

static int s_write_uint32(extract_zip_t* zip, uint32_t value)
{
    if (s_native_little_endinesss()) {
        return s_write(zip, &value, sizeof(value));
    }
    else {
        unsigned char value2[4] = { value, value>>8, value>>16, value>>24};
        return s_write(zip, &value2, sizeof(value2));
    }
}

static int s_write_uint16(extract_zip_t* zip, uint16_t value)
{
    if (s_native_little_endinesss()) {
        return s_write(zip, &value, sizeof(value));
    }
    else {
        unsigned char value2[2] = { value, value>>8};
        return s_write(zip, &value2, sizeof(value2));
    }
}

static int s_write_string(extract_zip_t* zip, const char* text)
{
    return s_write(zip, text, strlen(text));
}

int extract_zip_write_file(
        extract_zip_t* zip,
        const void* data,
        size_t data_length,
        const char* name
        )
{
    int e = -1;
    
    uint16_t    mtime = 0;
    uint16_t    mdate = 0;
    
    /* Create central directory file header for later. */
    extract_zip_cd_file_t*  cd_files = realloc(
            zip->cd_files,
            (zip->cd_files_num+1) * sizeof(extract_zip_cd_file_t)
            );
    if (!cd_files) goto end;
    zip->cd_files = cd_files;
    extract_zip_cd_file_t* cd_file = &cd_files[zip->cd_files_num];
    cd_file->mtime = mtime;
    cd_file->mdate = mdate;
    cd_file->crc_sum = crc32(0, NULL, 0);
    cd_file->crc_sum = crc32(cd_file->crc_sum, data, data_length);
    cd_file->size_compressed = data_length;
    cd_file->size_uncompressed = data_length;
    cd_file->name = strdup(name);
    cd_file->offset = extract_buffer_pos(zip->buffer);
    if (!cd_file->name) goto end;
    zip->cd_files_num += 1;
    
    /* Write local header. */
    s_write_uint32(zip, 0x04034b50);
    s_write_uint16(zip, 0);                             /* Version needed to extract (minimum) */
    s_write_uint16(zip, 0);                             /* General purpose bit flag */
    s_write_uint16(zip, 0);                             /* Compression method */
    s_write_uint16(zip, cd_file->mtime);                /* File last modification time */
    s_write_uint16(zip, cd_file->mdate);                /* File last modification date */
    s_write_uint32(zip, cd_file->crc_sum);              /* CRC-32 of uncompressed data */
    s_write_uint32(zip, cd_file->size_compressed);      /* Compressed size */
    s_write_uint32(zip, cd_file->size_uncompressed);    /* Uncompressed size */
    s_write_uint16(zip, strlen(name));                  /* File name length (n) */
    s_write_uint16(zip, 0);                             /* Extra field length (m) */
    s_write_string(zip, cd_file->name);                 /* File name */
    /* Write the (uncompressed) data. */
    s_write(zip, data, data_length);
    
    if (zip->errno_)    e = -1;
    else if (zip->eof)  e = +1;
    else e = 0;
    
    end:
    if (e) {
        if (cd_file) free(cd_file->name);
        free(cd_file);
    }
    
    return e;
}

int extract_zip_close(extract_zip_t* zip)
{
    int e = -1;
    
    long pos = extract_buffer_pos(zip->buffer);
    long len = 0;
    int i;
    
    /* Write Central directory file headers. */
    for (i=0; i<zip->cd_files_num; ++i) {
        long pos2 = extract_buffer_pos(zip->buffer);
        extract_zip_cd_file_t* cd_file = &zip->cd_files[i];
        s_write_uint32(zip, 0x02014b50);
        s_write_uint16(zip, 0);                             /* Version made by */
        s_write_uint16(zip, 0);                             /* Version needed to extract (minimum) */
        s_write_uint16(zip, 0);                             /* General purpose bit flag */
        s_write_uint16(zip, 0);                             /* Compression method */
        s_write_uint16(zip, cd_file->mtime);                /* File last modification time */
        s_write_uint16(zip, cd_file->mdate);                /* File last modification date */
        s_write_uint32(zip, cd_file->crc_sum);              /* CRC-32 of uncompressed data */
        s_write_uint32(zip, cd_file->size_compressed);      /* Compressed size */
        s_write_uint32(zip, cd_file->size_uncompressed);    /* Uncompressed size */
        s_write_uint16(zip, strlen(cd_file->name));         /* File name length (n) */
        s_write_uint16(zip, 0);                             /* Extra field length (m) */
        s_write_uint16(zip, 0);                             /* File comment length (k) */
        s_write_uint16(zip, 0);                             /* Disk number where file starts */
        s_write_uint16(zip, 0);                             /* Internal file attributes */
        s_write_uint32(zip, 0);                             /* External file attributes */
        s_write_uint32(zip, cd_file->offset);               /* Relative offset of local file header. This is the number of bytes between the start of the first disk on which the file occurs, and the start of the local file header. This allows software reading the central directory to locate the position of the file inside the ZIP file. */
        s_write_string(zip, cd_file->name);                 /* File name */
        len += extract_buffer_pos(zip->buffer) - pos2;
    }
    
    /* Write End of central directory record. */
    s_write_uint32(zip, 0x06054b50);
    s_write_uint16(zip, 0);                  /* Number of this disk */
    s_write_uint16(zip, 0);                  /* Disk where central directory starts */
    s_write_uint16(zip, zip->cd_files_num);  /* Number of central directory records on this disk */
    s_write_uint16(zip, zip->cd_files_num);  /* Total number of central directory records */
    s_write_uint32(zip, len);                /* Size of central directory (bytes) */
    s_write_uint32(zip, pos);                /* Offset of start of central directory, relative to start of archive */
    
    char comment[] = "MuPDF";
    s_write_uint32(zip, sizeof(comment)-1); /* Comment length (n) */
    s_write_string(zip, comment);
    
    if (zip->errno_)    e = -1;
    else if (zip->eof)  e = +1;
    else e = 0;
    
    return e;
}

/*
int main(int argc, char** argv)
{
    (void) argc;
    (void) argv;
    int e = -1;
    extract_zip_t* zip;
    if (extract_zip_open("ziptest.zip", "w", &zip)) goto end;
    const char hw[] = "hello world\n";
    if (extract_zip_write_file(zip, hw, sizeof(hw)-1, "ziptest/hw.txt")) goto end;
    if (extract_zip_write_file(zip, hw, sizeof(hw)-1, "ziptest/foo/hw2.txt")) goto end;
    if (extract_zip_close(zip)) goto end;
    
    e = 0;
    
    end:
    
    if (e) {
        printf("failed: %s\n", strerror(errno));
        return 1;
    }
    return 0;
}
*/
