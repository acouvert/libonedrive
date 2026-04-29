/*
 * test_http_client.c — unit tests for http_client.c
 *
 * Links against the REAL http_client.c (with curl).
 * Each test function returns 0 on success, -1 on failure.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "http_client.h"

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
/*  Tests: create / destroy                                            */
/* ------------------------------------------------------------------ */

static int test_http_client_create_destroy(void)
{
    HttpClient* h = http_client_create();
    ASSERT(h != NULL);
    http_client_destroy(h);
    return 0;
}

static int test_http_client_destroy_null(void)
{
    http_client_destroy(NULL); /* must not crash */
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Tests: url_encode                                                  */
/* ------------------------------------------------------------------ */

static int test_url_encode_basic(void)
{
    HttpClient* h = http_client_create();
    ASSERT(h != NULL);
    Buffer out = {0};
    ASSERT(http_client_url_encode(h, "hello world", &out) == 0);
    ASSERT(out.data != NULL);
    ASSERT(strcmp(out.data, "hello%20world") == 0);
    buffer_free(&out);
    http_client_destroy(h);
    return 0;
}

static int test_url_encode_empty_string(void)
{
    HttpClient* h = http_client_create();
    ASSERT(h != NULL);
    Buffer out = {0};
    ASSERT(http_client_url_encode(h, "", &out) == 0);
    ASSERT(out.data != NULL);
    ASSERT(out.data[0] == '\0');
    buffer_free(&out);
    http_client_destroy(h);
    return 0;
}

static int test_url_encode_null_client(void)
{
    Buffer out = {0};
    ASSERT(http_client_url_encode(NULL, "x", &out) == -1);
    return 0;
}

static int test_url_encode_null_str(void)
{
    HttpClient* h = http_client_create();
    ASSERT(h != NULL);
    Buffer out = {0};
    ASSERT(http_client_url_encode(h, NULL, &out) == -1);
    http_client_destroy(h);
    return 0;
}

static int test_url_encode_null_out(void)
{
    HttpClient* h = http_client_create();
    ASSERT(h != NULL);
    ASSERT(http_client_url_encode(h, "x", NULL) == -1);
    http_client_destroy(h);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Tests: redirect URL                                                */
/* ------------------------------------------------------------------ */

static int test_redirect_url_null_client(void)
{
    char* out = NULL;
    ASSERT(http_client_get_redirect_url(NULL, &out) == -1);
    return 0;
}

static int test_redirect_url_null_out(void)
{
    HttpClient* h = http_client_create();
    ASSERT(h != NULL);
    ASSERT(http_client_get_redirect_url(h, NULL) == -1);
    http_client_destroy(h);
    return 0;
}

static int test_redirect_url_no_request(void)
{
    HttpClient* h = http_client_create();
    ASSERT(h != NULL);
    char* out = NULL;
    /* No prior request — no redirect URL available */
    ASSERT(http_client_get_redirect_url(h, &out) == -1);
    http_client_destroy(h);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Tests: http_form_add_param                                                    */
/* ------------------------------------------------------------------ */

static int test_form_add_single(void)
{
    HttpClient* h = http_client_create();
    ASSERT(h != NULL);
    Buffer f = {0};
    http_form_add_param(h, "key", "value", &f);
    ASSERT(f.data != NULL);
    ASSERT(strcmp(f.data, "key=value") == 0);
    free(f.data);
    http_client_destroy(h);
    return 0;
}

static int test_form_add_multiple(void)
{
    HttpClient* h = http_client_create();
    ASSERT(h != NULL);
    Buffer f = {0};
    http_form_add_param(h, "a", "1", &f);
    http_form_add_param(h, "b", "2", &f);
    ASSERT(f.data != NULL);
    ASSERT(strcmp(f.data, "a=1&b=2") == 0);
    free(f.data);
    http_client_destroy(h);
    return 0;
}

static int test_form_add_encodes_value(void)
{
    HttpClient* h = http_client_create();
    ASSERT(h != NULL);
    Buffer f = {0};
    http_form_add_param(h, "q", "hello world", &f);
    ASSERT(f.data != NULL);
    ASSERT(strcmp(f.data, "q=hello%20world") == 0);
    free(f.data);
    http_client_destroy(h);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Tests: http_url_get_param                         */
/* ------------------------------------------------------------------ */

static int test_extract_param_basic(void)
{
    HttpClient* h = http_client_create();
    ASSERT(h != NULL);
    char* out = NULL;
    ASSERT(http_url_get_param(h, "https://example.com/path?token=abc123", "token", &out) == 0);
    ASSERT(out != NULL);
    ASSERT(strcmp(out, "abc123") == 0);
    free(out);
    http_client_destroy(h);
    return 0;
}

static int test_extract_param_first_param(void)
{
    HttpClient* h = http_client_create();
    ASSERT(h != NULL);
    char* out = NULL;
    ASSERT(http_url_get_param(h, "https://example.com?a=1&b=2", "a", &out) == 0);
    ASSERT(strcmp(out, "1") == 0);
    free(out);
    http_client_destroy(h);
    return 0;
}

static int test_extract_param_middle_param(void)
{
    HttpClient* h = http_client_create();
    ASSERT(h != NULL);
    char* out = NULL;
    ASSERT(http_url_get_param(h, "https://example.com?a=1&token=xyz&c=3", "token", &out) == 0);
    ASSERT(strcmp(out, "xyz") == 0);
    free(out);
    http_client_destroy(h);
    return 0;
}

static int test_extract_param_not_found(void)
{
    HttpClient* h = http_client_create();
    ASSERT(h != NULL);
    char* out = NULL;
    ASSERT(http_url_get_param(h, "https://example.com?a=1", "missing", &out) == -1);
    http_client_destroy(h);
    return 0;
}

static int test_extract_param_prefix_no_match(void)
{
    /* "token" should not match "deltatoken" */
    HttpClient* h = http_client_create();
    ASSERT(h != NULL);
    char* out = NULL;
    ASSERT(http_url_get_param(h, "https://example.com?deltatoken=bad&token=good", "token", &out) == 0);
    ASSERT(strcmp(out, "good") == 0);
    free(out);
    http_client_destroy(h);
    return 0;
}

static int test_extract_param_null_url(void)
{
    HttpClient* h = http_client_create();
    ASSERT(h != NULL);
    char* out = NULL;
    ASSERT(http_url_get_param(h, NULL, "token", &out) == -1);
    http_client_destroy(h);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  main                                                               */
/* ------------------------------------------------------------------ */

int main(void)
{
    int passed = 0, failed = 0;

    /* create / destroy */
    RUN_TEST(test_http_client_create_destroy);
    RUN_TEST(test_http_client_destroy_null);

    /* url_encode */
    RUN_TEST(test_url_encode_basic);
    RUN_TEST(test_url_encode_empty_string);
    RUN_TEST(test_url_encode_null_client);
    RUN_TEST(test_url_encode_null_str);
    RUN_TEST(test_url_encode_null_out);

    /* redirect URL */
    RUN_TEST(test_redirect_url_null_client);
    RUN_TEST(test_redirect_url_null_out);
    RUN_TEST(test_redirect_url_no_request);

    /* http_form_add_param */
    RUN_TEST(test_form_add_single);
    RUN_TEST(test_form_add_multiple);
    RUN_TEST(test_form_add_encodes_value);

    /* http_url_get_param */
    RUN_TEST(test_extract_param_basic);
    RUN_TEST(test_extract_param_first_param);
    RUN_TEST(test_extract_param_middle_param);
    RUN_TEST(test_extract_param_not_found);
    RUN_TEST(test_extract_param_prefix_no_match);
    RUN_TEST(test_extract_param_null_url);

    fprintf(stderr, "  %-50s%s\n", "---", "--");
    fprintf(stderr, "  %d passed, %d failed\n\n", passed, failed);

    return failed;
}
