#ifndef _BUFFER_H_
#define _BUFFER_H_

#include <stddef.h>

typedef struct {
    char*  data; /* heap-allocated body, null-terminated */
    size_t size; /* number of bytes currently stored (excluding '\0') */
    size_t cap;  /* total allocated capacity of data */
} Buffer;

/* Allocate buffer->data to hold size bytes.
 * size must be > 0.  Any existing data is freed first.
 * Returns 0 on success, -1 on failure. */
int buffer_alloc(Buffer* buffer, size_t size);

/* Ensure buffer->data has at least new_cap bytes of capacity.
 * new_cap must be > 0.  Uses geometric growth to amortize repeated
 * small appends.  Does nothing if the buffer already has enough capacity.
 * Returns 0 on success, -1 on failure. */
int buffer_reserve(Buffer* buffer, size_t new_cap);

/* Free buffer->data and reset all fields to zero.
 * Safe to call on an already-freed or zero-initialized buffer. */
void buffer_free(Buffer* buffer);

/* Reset size to zero, keeping the existing allocation.
 * Returns 0 on success, -1 if buffer is NULL. */
int buffer_reset(Buffer* buffer);

/* Append size bytes from data into the buffer at the current offset.
 * A null terminator is appended.  Reserves capacity if needed.
 * Returns 0 on success, -1 on failure. */
int buffer_append(Buffer* buffer, const char* data, size_t size);

/* Append formatted output at the current offset.
 * Automatically extends the buffer to fit the result.
 * A null terminator is appended.  Can be called on a zero-initialized
 * Buffer (allocation is handled internally).
 * Returns 0 on success, -1 on failure. */
__attribute__((format(printf, 2, 3)))
int buffer_printf(Buffer* buf, const char* fmt, ...);

#endif /* _BUFFER_H_ */
