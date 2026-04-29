#include "buffer.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int buffer_alloc(Buffer* buffer, size_t size)
{
    if(!buffer || size == 0)
    {
        return -1;
    }

    if (buffer->data)
    {
        free(buffer->data);
    }

    buffer->size = 0;
    buffer->cap = 0;

    buffer->data = malloc(size);
    if (!buffer->data)
    {
        return -1;
    }

    buffer->cap = size;
    return 0;
}

int buffer_reserve(Buffer* buffer, size_t new_cap)
{
    if (!buffer || new_cap == 0)
    {
        return -1;
    }

    if (new_cap > buffer->cap)
    {
        /* Geometric growth: double the current capacity (minimum 64 bytes)
         * to amortize repeated small appends.  Fall back to the exact
         * requested size when a single large allocation exceeds the
         * doubled value. */
        size_t grow = buffer->cap < 64 ? 64 : buffer->cap * 2;
        if (grow < new_cap)
        {
            grow = new_cap;
        }

        char* data = realloc(buffer->data, grow);
        if (!data)
        {
            return -1;
        }

        buffer->data = data;
        buffer->cap = grow;
    }

    return 0;
}

void buffer_free(Buffer* buffer)
{
    free(buffer->data);
    buffer->data = NULL;
    buffer->size = 0;
    buffer->cap = 0;
}

int buffer_reset(Buffer* buffer)
{
    if (!buffer)
    {
        return -1;
    }

    buffer->size = 0;
    return 0;
}

int buffer_append(Buffer* buffer, const char* data, size_t size)
{
    if(!buffer || !data)
    {
        return -1;
    }

    size_t required_size = size + 1; /* for null terminator */
    if (buffer_reserve(buffer, buffer->size + required_size) != 0)
    {
        return -1;
    }

    memcpy(buffer->data + buffer->size, data, size);
    buffer->size += size;
    buffer->data[buffer->size] = '\0';

    return 0;
}

int buffer_printf(Buffer* buffer, const char* fmt, ...)
{
    if (!buffer || !fmt)
    {
        return -1;
    }

    va_list ap;

    /* First pass: measure how many bytes are needed. */
    va_start(ap, fmt);
    int n = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);

    if (n < 0)
    {
        return -1;
    }

    n += 1; /* for null terminator */

    if (buffer_reserve(buffer,  buffer->size + (size_t)n) != 0)
    {
        return -1;
    }

    /* Second pass: write into the buffer. */
    va_start(ap, fmt);
    n = vsnprintf(buffer->data + buffer->size, buffer->cap - buffer->size, fmt, ap);
    va_end(ap);

    if (n < 0)
    {
        return -1;
    }

    buffer->size += (size_t)n;
    buffer->data[buffer->size] = '\0';

    return 0;
}
