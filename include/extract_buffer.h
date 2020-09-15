#ifndef ARTIFEX_EXTRACT_BUFFER_H
#define ARTIFEX_EXTRACT_BUFFER_H

#include <stddef.h>


/* Reading and writing abstractions. */


typedef struct extract_buffer_t extract_buffer_t;
/* Abstract state for a buffer. */


static inline int extract_buffer_read(
        extract_buffer_t*   buffer,
        void*               data,
        size_t              numbytes,
        size_t*             o_actual
        );
/* Reads specified number of bytes from buffer into data..+bytes. Returns +1 if
short read due to EOF.

buffer:
    As returned by earlier call to extract_buffer_open().
data:
    Location of transferred data.
bytes:
    Number of bytes transferred.
o_actual:
    Optional out-param, set to actual number of bytes read.
*/


static inline int extract_buffer_write(
        extract_buffer_t*   buffer,
        const void*         data,
        size_t              numbytes,
        size_t*             o_actual
        );
/* Writes specified data into buffer. Returns +1 if short write due to EOF.

buffer:
    As returned by earlier call to extract_buffer_open().
data:
    Location of source data.
bytes:
    Number of bytes to copy.
out_actual:
    Optional out-param, set to actual number of bytes read. Can be negative if
    internal cache-flush using fn_write() fails or returns EOF.
*/


size_t extract_buffer_pos(extract_buffer_t* buffer);
/* Returns number of bytes read or number of bytes written so far. */


int extract_buffer_close(extract_buffer_t** io_buffer);
/* Closes down an extract_buffer_t and frees all internal resources.

Can return error or +1 for EOF if write buffer and fn_write() fails when
flushing cache.

Always sets *io_buffer to NULL. Does nothing if *io_buffer is already NULL.
*/


typedef int (*extract_buffer_fn_read)(void* handle, void* destination, size_t numbytes, size_t* o_actual);
/* Callback used by read buffer. Should read data from buffer into the supplied
destination. E.g. this is used to fill cache or to handle large reads.

Should returns 0 on success (including EOF) or -1 with errno set.

handle:
    As passed to extract_buffer_open().
destination:
    Start of destination.
bytes:
    Number of bytes in destination.
o_actual:
    Out-param, set to actual number of bytes transferred or zero if EOF. Short
    reads are not an error.
*/

typedef int (*extract_buffer_fn_write)(void* handle, const void* source, size_t numbytes, size_t* o_actual);
/* Callback used by write buffer. Should write data from the supplied source
into the buffer. E.g. used to flush cache or to handle large writes.

Should return 0 on success (including EOF) or -1 with errno set.

handle:
    As passed to extract_buffer_open().
source:
    Start of source.
bytes:
    Number of bytes in source.
o_actual:
    Out-param, set to actual number of bytes transferred or zero if EOF. Short
    writes are not an error.
*/

typedef int (*extract_buffer_fn_cache)(void* handle, void** o_cache, size_t* o_numbytes);
/* Callback to flush/populate cache.

If the buffer is for writing:
    Should return a memory region to which data can be written. Any data
    written to a previous cache will have already been passed to fn_write() so
    this can overlap or be the same as any previously-returned cache.

If the buffer is for reading:
    Should return a memory region containing more data to be read. All data in
    any previously-returned cache has been read so this can overlap or be the
    same as any previous cache.

handle:
    As passed to extract_buffer_open().
o_data:
    Out-param, set to point to new cache.
o_numbytes:
    Out-param, set to size of new cache.

If no data is available due to EOF, should return with *o_numbytes set to zero.
*/

typedef void (*extract_buffer_fn_close)(void* handle);
/* Called by extract_buffer_close().

handle:
    As passed to extract_buffer_open().
*/


int extract_buffer_open(
        void*                   handle,
        extract_buffer_fn_read  fn_read,
        extract_buffer_fn_write fn_write,
        extract_buffer_fn_cache fn_cache,
        extract_buffer_fn_close fn_close,
        extract_buffer_t**      o_buffer
        );
/* Creates an extract_buffer_t that uses specified callbacks.

If fn_read is non-NULL the buffer is a read buffer, else if fn_write is
non-NULL the buffer is a write buffer. Passing non-NULL for both or neither is
not supported.

handle:
    Passed to fn_read, fn_write, fn_cache and fn_close callbacks.
fn_read:
    Callback for reading data.
fn_write:
    Callback for writing data.
fn_cache:
    Optional cache calback.
fn_close:
    Optional close callback.
o_buffer:
    Out-param. Set to NULL on error.
*/


int extract_buffer_open_simple(
        char*                   data,
        size_t                  numbytes,
        void*                   handle,
        extract_buffer_fn_close fn_close,
        extract_buffer_t**      o_buffer
        );
/* Creates an extract_buffer_t that reads from a single fixed block of memory.

The data is not copied so data..+data_length must exist for the lifetime of the
returned extract_buffer_t.

data:
    Start of memory region.
bytes:
    Size of memory region.
handle:
    Passed to fn_close.
fn_close:
    Optional callback called by extract_buffer_close().
o_buffer:
    Out-param.
*/


int extract_buffer_open_file(const char* path, int writable, extract_buffer_t** o_buffer);
/* Creates an read buffer that reads from or writes to a file. Uses a
fixed-size internal cache.

path:
    Path of file to read from.
writable:
    We create read buffer if zero, else a write buffer.
o_buffer:
    Out-param. Set to NULL on error.
*/


/* Include implementations of inline-functions. */

#include "extract_buffer_impl.h"

#endif
