/*
 * test_buffer.c — unit tests for buffer.c
 *
 * Each test function returns 0 on success, -1 on failure.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "buffer.h"

/* ------------------------------------------------------------------ */
/*  Helpers                                                            */
/* ------------------------------------------------------------------ */

#define ASSERT(expr) do { \
    if (!(expr)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); return -1; } \
} while (0)

#define RUN_TEST(fn) do { \
    fprintf(stderr, "  %-50s", #fn); \
    if (fn() == 0) { passed++; fprintf(stderr, "OK\n"); } \
    else           { failed++; fprintf(stderr, "\n"); } \
} while (0)

/* ------------------------------------------------------------------ */
/*  Tests: buffer_alloc                                                */
/* ------------------------------------------------------------------ */

static int test_alloc_basic(void)
{
    Buffer buf = {0};
    ASSERT(buffer_alloc(&buf, 64) == 0);
    ASSERT(buf.data != NULL);
    ASSERT(buf.cap == 64);
    ASSERT(buf.size == 0);
    buffer_free(&buf);
    return 0;
}

static int test_alloc_null_buffer(void)
{
    ASSERT(buffer_alloc(NULL, 64) == -1);
    return 0;
}

static int test_alloc_zero_size(void)
{
    Buffer buf = {0};
    ASSERT(buffer_alloc(&buf, 0) == -1);
    return 0;
}

static int test_alloc_frees_existing(void)
{
    Buffer buf = {0};
    ASSERT(buffer_alloc(&buf, 32) == 0);
    buf.data[0] = 'x';
    /* Second alloc should free the first and reset */
    ASSERT(buffer_alloc(&buf, 64) == 0);
    ASSERT(buf.cap == 64);
    ASSERT(buf.size == 0);
    buffer_free(&buf);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Tests: buffer_reserve                                               */
/* ------------------------------------------------------------------ */

static int test_extend_grows(void)
{
    Buffer buf = {0};
    ASSERT(buffer_alloc(&buf, 8) == 0);
    ASSERT(buffer_reserve(&buf, 100) == 0);
    ASSERT(buf.cap >= 100);
    buffer_free(&buf);
    return 0;
}

static int test_extend_noop_if_enough(void)
{
    Buffer buf = {0};
    ASSERT(buffer_alloc(&buf, 256) == 0);
    size_t old_cap = buf.cap;
    ASSERT(buffer_reserve(&buf, 10) == 0);
    ASSERT(buf.cap == old_cap);
    buffer_free(&buf);
    return 0;
}

static int test_extend_null_buffer(void)
{
    ASSERT(buffer_reserve(NULL, 10) == -1);
    return 0;
}

static int test_extend_zero_size(void)
{
    Buffer buf = {0};
    /* Reserving 0 capacity is rejected */
    ASSERT(buffer_reserve(&buf, 0) == -1);
    return 0;
}

static int test_extend_from_zero(void)
{
    Buffer buf = {0};
    ASSERT(buffer_reserve(&buf, 32) == 0);
    ASSERT(buf.data != NULL);
    ASSERT(buf.cap >= 32);
    buffer_free(&buf);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Tests: buffer_free                                                 */
/* ------------------------------------------------------------------ */

static int test_free_resets_fields(void)
{
    Buffer buf = {0};
    buffer_alloc(&buf, 64);
    buf.size = 10;
    buffer_free(&buf);
    ASSERT(buf.data == NULL);
    ASSERT(buf.size == 0);
    ASSERT(buf.cap == 0);
    return 0;
}

static int test_free_zero_initialized(void)
{
    Buffer buf = {0};
    buffer_free(&buf); /* must not crash */
    ASSERT(buf.data == NULL);
    return 0;
}

static int test_free_double_free(void)
{
    Buffer buf = {0};
    buffer_alloc(&buf, 64);
    buffer_free(&buf);
    buffer_free(&buf); /* must not crash */
    ASSERT(buf.data == NULL);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Tests: buffer_append                                                 */
/* ------------------------------------------------------------------ */

static int test_copy_basic(void)
{
    Buffer buf = {0};
    ASSERT(buffer_append(&buf, "hello", 5) == 0);
    ASSERT(buf.data != NULL);
    ASSERT(buf.size == 5);
    ASSERT(strcmp(buf.data, "hello") == 0);
    buffer_free(&buf);
    return 0;
}

static int test_copy_appends(void)
{
    Buffer buf = {0};
    ASSERT(buffer_append(&buf, "abc", 3) == 0);
    ASSERT(buffer_append(&buf, "def", 3) == 0);
    ASSERT(buf.size == 6);
    ASSERT(strcmp(buf.data, "abcdef") == 0);
    buffer_free(&buf);
    return 0;
}

static int test_copy_null_buffer(void)
{
    ASSERT(buffer_append(NULL, "x", 1) == -1);
    return 0;
}

static int test_copy_null_data(void)
{
    Buffer buf = {0};
    ASSERT(buffer_append(&buf, NULL, 5) == -1);
    return 0;
}

static int test_copy_auto_extends(void)
{
    Buffer buf = {0};
    ASSERT(buffer_alloc(&buf, 4) == 0);
    /* "hello" needs 6 bytes (5 + null), but cap is 4 */
    ASSERT(buffer_append(&buf, "hello", 5) == 0);
    ASSERT(buf.size == 5);
    ASSERT(strcmp(buf.data, "hello") == 0);
    buffer_free(&buf);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Tests: buffer_printf                                               */
/* ------------------------------------------------------------------ */

static int test_printf_basic(void)
{
    Buffer buf = {0};
    ASSERT(buffer_printf(&buf, "hello %s", "world") == 0);
    ASSERT(buf.data != NULL);
    ASSERT(strcmp(buf.data, "hello world") == 0);
    ASSERT(buf.size == 11);
    buffer_free(&buf);
    return 0;
}

static int test_printf_int(void)
{
    Buffer buf = {0};
    ASSERT(buffer_printf(&buf, "%d", 42) == 0);
    ASSERT(strcmp(buf.data, "42") == 0);
    buffer_free(&buf);
    return 0;
}

static int test_printf_appends(void)
{
    Buffer buf = {0};
    ASSERT(buffer_printf(&buf, "a=%d", 1) == 0);
    ASSERT(buffer_printf(&buf, "&b=%d", 2) == 0);
    ASSERT(strcmp(buf.data, "a=1&b=2") == 0);
    ASSERT(buf.size == 7);
    buffer_free(&buf);
    return 0;
}

static int test_printf_null_buffer(void)
{
    ASSERT(buffer_printf(NULL, "x") == -1);
    return 0;
}

static int test_printf_null_fmt(void)
{
    Buffer buf = {0};
    ASSERT(buffer_printf(&buf, NULL) == -1);
    return 0;
}

static int test_printf_empty_format(void)
{
    Buffer buf = {0};
    const char* empty = "";
    ASSERT(buffer_printf(&buf, "%s", empty) == 0);
    ASSERT(buf.data != NULL);
    ASSERT(buf.data[0] == '\0');
    ASSERT(buf.size == 0);
    buffer_free(&buf);
    return 0;
}

static int test_printf_auto_allocs(void)
{
    /* Start from zero-initialized buffer — printf must allocate internally */
    Buffer buf = {0};
    ASSERT(buffer_printf(&buf, "https://example.com/%s?id=%d", "path", 123) == 0);
    ASSERT(strcmp(buf.data, "https://example.com/path?id=123") == 0);
    buffer_free(&buf);
    return 0;
}

static int test_printf_mixed_with_copy(void)
{
    Buffer buf = {0};
    ASSERT(buffer_append(&buf, "prefix:", 7) == 0);
    ASSERT(buffer_printf(&buf, "%d", 99) == 0);
    ASSERT(strcmp(buf.data, "prefix:99") == 0);
    ASSERT(buf.size == 9);
    buffer_free(&buf);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Tests: buffer_reset                                                */
/* ------------------------------------------------------------------ */

static int test_reset_basic(void)
{
    Buffer buf = {0};
    ASSERT(buffer_append(&buf, "hello", 5) == 0);
    ASSERT(buf.size == 5);
    ASSERT(buffer_reset(&buf) == 0);
    ASSERT(buf.size == 0);
    ASSERT(buf.cap > 0); /* allocation preserved */
    ASSERT(buf.data != NULL);
    buffer_free(&buf);
    return 0;
}

static int test_reset_null_buffer(void)
{
    ASSERT(buffer_reset(NULL) == -1);
    return 0;
}

static int test_reset_then_reuse(void)
{
    Buffer buf = {0};
    ASSERT(buffer_append(&buf, "first", 5) == 0);
    ASSERT(buffer_reset(&buf) == 0);
    ASSERT(buffer_append(&buf, "second", 6) == 0);
    ASSERT(strcmp(buf.data, "second") == 0);
    ASSERT(buf.size == 6);
    buffer_free(&buf);
    return 0;
}

static int test_reset_zero_initialized(void)
{
    Buffer buf = {0};
    ASSERT(buffer_reset(&buf) == 0);
    ASSERT(buf.size == 0);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  main                                                               */
/* ------------------------------------------------------------------ */

int main(void)
{
    int passed = 0, failed = 0;

    /* buffer_alloc */
    RUN_TEST(test_alloc_basic);
    RUN_TEST(test_alloc_null_buffer);
    RUN_TEST(test_alloc_zero_size);
    RUN_TEST(test_alloc_frees_existing);

    /* buffer_reserve */
    RUN_TEST(test_extend_grows);
    RUN_TEST(test_extend_noop_if_enough);
    RUN_TEST(test_extend_null_buffer);
    RUN_TEST(test_extend_zero_size);
    RUN_TEST(test_extend_from_zero);

    /* buffer_free */
    RUN_TEST(test_free_resets_fields);
    RUN_TEST(test_free_zero_initialized);
    RUN_TEST(test_free_double_free);

    /* buffer_append */
    RUN_TEST(test_copy_basic);
    RUN_TEST(test_copy_appends);
    RUN_TEST(test_copy_null_buffer);
    RUN_TEST(test_copy_null_data);
    RUN_TEST(test_copy_auto_extends);

    /* buffer_printf */
    RUN_TEST(test_printf_basic);
    RUN_TEST(test_printf_int);
    RUN_TEST(test_printf_appends);
    RUN_TEST(test_printf_null_buffer);
    RUN_TEST(test_printf_null_fmt);
    RUN_TEST(test_printf_empty_format);
    RUN_TEST(test_printf_auto_allocs);
    RUN_TEST(test_printf_mixed_with_copy);

    /* buffer_reset */
    RUN_TEST(test_reset_basic);
    RUN_TEST(test_reset_null_buffer);
    RUN_TEST(test_reset_then_reuse);
    RUN_TEST(test_reset_zero_initialized);

    fprintf(stderr, "  %-50s%s\n", "---", "--");
    fprintf(stderr, "  %d passed, %d failed\n\n", passed, failed);

    return failed;
}
