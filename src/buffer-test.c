#include "../include/extract_buffer.h"

#include "outf.h"

#include <assert.h>
#include <stdlib.h>


static int rand_int(int max)
/* Returns random int from 0..max-1. */
{
    return (int) (rand() / (RAND_MAX+1.0) * max);
}


/* Support for an extract_buffer_t that reads from / writes to a fixed block of
memory, with a fn_cache() that returns a randomly-sized cache each time it is
called, and read/write functions that do random short reads and writes. */

typedef struct
{
    char*   data;
    size_t  bytes;  /* Size of data[]. */
    size_t  pos;    /* Current position in data[]. */
    char    cache[137];
    int     num_calls_cache;
    int     num_calls_read;
    int     num_calls_write;
} mem_t;

static int s_read(void* handle, void* destination, size_t bytes, size_t* o_actual)
/* Does a randomised short read. */
{
    assert(bytes > 0);
    mem_t* r = handle;
    r->num_calls_read += 1;
    assert(r->pos <= r->bytes);
    size_t n = 91;
    if (n > bytes) n = bytes;
    if (n > r->bytes - r->pos) n = r->bytes - r->pos;
    if (n) n = rand_int(n-1) + 1;
    memcpy(destination, r->data + r->pos, n);
    r->pos += n;
    *o_actual = n;
    return 0;
}

static int s_read_cache(void* handle, void** o_cache, size_t* o_numbytes)
/* Returns a cache with randomised size. */
{
    mem_t* r = handle;
    r->num_calls_cache += 1;
    *o_cache = r->cache;
    int n = r->bytes - r->pos;
    if (n > (int) sizeof(r->cache)) n = sizeof(r->cache);
    if (n) n = rand_int( n - 1) + 1;
    memcpy(r->cache, r->data + r->pos, n);
    r->pos += n;
    *o_cache = r->cache;
    *o_numbytes = n;
    return 0;
}

static void s_create_read_buffer(int bytes, mem_t* r, extract_buffer_t** o_buffer)
/* Creates extract_buffer_t that reads from randomised data using randomised
short reads and cache with randomised sizes. */
{
    int i;
    int e;
    r->data = malloc(bytes);
    for (i=0; i<bytes; ++i) {
        r->data[i] = rand();
    }
    r->bytes = bytes;
    r->pos = 0;
    r->num_calls_cache = 0;
    r->num_calls_read = 0;
    r->num_calls_write = 0;
    e = extract_buffer_open(r, s_read, NULL /*write*/, s_read_cache, NULL /*close*/, o_buffer);
    assert(!e);
}

static void test_read(void)
{
    /* Create read buffer with randomised content. */
    size_t len = 12345;
    mem_t r;
    extract_buffer_t* buffer;
    s_create_read_buffer(len, &r, &buffer);
        
    /* Repeatedly read from read-buffer until we get EOF, and check we read the
    original content. */
    char* out_buffer = malloc(len);
    size_t out_pos = 0;
    int its;
    for (its=0;; ++its) {
        size_t actual;
        size_t n = rand_int(120)+1;
        int e = extract_buffer_read(buffer, out_buffer + out_pos, n, &actual);
        out_pos += actual;
        assert(out_pos == extract_buffer_pos(buffer));
        if (e == 1) break;
        assert(!e);
        assert(!memcmp(out_buffer, r.data, out_pos));
    }
    assert(out_pos == len);
    assert(!memcmp(out_buffer, r.data, len));
    outf("its=%i num_calls_read=%i num_calls_write=%i num_calls_cache=%i",
            its, r.num_calls_read, r.num_calls_write, r.num_calls_cache);
    outf("Read test passed.\n");
}


static int s_write(void* handle, const void* source, size_t bytes, size_t* o_actual)
/* Does a randomised short write. */
{
    mem_t* r = handle;
    r->num_calls_write += 1;
    size_t n = 61;
    if (n > bytes) n = bytes;
    if (n > r->bytes - r->pos) n = r->bytes - r->pos;
    assert(n);
    n = rand_int(n-1) + 1;
    memcpy(r->data + r->pos, source, n);
    r->data[r->bytes] = 0;
    r->pos += n;
    *o_actual = n;
    return 0;
}

static int s_write_cache(void* handle, void** o_cache, size_t* o_numbytes)
/* Returns a cache with randomised size. */
{
    mem_t* r = handle;
    r->num_calls_cache += 1;
    assert(r->bytes >= r->pos);
    *o_cache = r->cache;
    int n = r->bytes - r->pos;
    if (n > (int) sizeof(r->cache)) n = sizeof(r->cache);
    if (n) n = rand_int( n - 1) + 1;
    *o_cache = r->cache;
    *o_numbytes = n;
    /* We will return a zero-length cache at EOF. */
    return 0;
}

static void s_create_write_buffer(size_t bytes, mem_t* r, extract_buffer_t** o_buffer)
/* Creates extract_buffer_t that reads from randomised data using randomised
short reads and cache with randomised sizes. */
{
    int e;
    r->data = malloc(bytes+1);
    bzero(r->data, bytes);
    r->bytes = bytes;
    r->pos = 0;
    r->num_calls_cache = 0;
    r->num_calls_read = 0;
    r->num_calls_write = 0;
    e = extract_buffer_open(r, NULL /*read*/, s_write, s_write_cache, NULL /*close*/, o_buffer);
    assert(!e);
}


static void test_write(void)
{
    /* Create write buffer. */
    size_t len = 12345;
    mem_t r;
    extract_buffer_t* buffer;
    s_create_write_buffer(len, &r, &buffer);
    
    /* Write to read-buffer, and check it contains the original content. */
    char* out_buffer = malloc(len);
    unsigned i;
    for (i=0; i<len; ++i) {
        out_buffer[i] = 'a' + rand_int(26);
    }
    size_t out_pos = 0;
    int its;
    for (its=0;; ++its) {
        size_t actual;
        size_t n = rand_int(12)+1;
        int e = extract_buffer_write(buffer, out_buffer+out_pos, n, &actual);
        out_pos += actual;
        assert(out_pos == extract_buffer_pos(buffer));
        if (e == 1) break;
        assert(!e);
    }
    int e = extract_buffer_close(&buffer);
    assert(!e);
    assert(out_pos == len);
    assert(!memcmp(out_buffer, r.data, len));
    outf("its=%i num_calls_read=%i num_calls_write=%i num_calls_cache=%i",
            its, r.num_calls_read, r.num_calls_write, r.num_calls_cache);
    outf("Write test passed.\n");
}


int main(void)
{
    outf_level_set(1);
    test_read();
    test_write();
    return 0;
}
