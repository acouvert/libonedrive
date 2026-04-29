/*
 * test_http_utils.c — unit tests for json_utils.c
 *
 * Links against mock_http_client.c.
 * Each test function returns 0 on success, -1 on failure.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "json_utils.h"
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
/*  Tests: json_doc_get_string                                         */
/* ------------------------------------------------------------------ */

static int test_json_get_string_basic(void)
{
    const char* json = "{\"name\": \"alice\"}";
    JsonDoc* doc = json_parse(json);
    ASSERT(doc != NULL);
    char* val = NULL;
    ASSERT(json_doc_get_string(doc, "name", &val) == 0);
    ASSERT(val != NULL);
    ASSERT(strcmp(val, "alice") == 0);
    free(val);
    json_free(doc);
    return 0;
}

static int test_json_get_string_missing_key(void)
{
    JsonDoc* doc = json_parse("{\"name\": \"alice\"}");
    ASSERT(doc != NULL);
    char* val = NULL;
    ASSERT(json_doc_get_string(doc, "age", &val) != 0);
    ASSERT(val == NULL);
    json_free(doc);
    return 0;
}

static int test_json_get_string_int_value(void)
{
    /* value is an integer, not a string */
    JsonDoc* doc = json_parse("{\"count\": 42}");
    ASSERT(doc != NULL);
    char* val = NULL;
    ASSERT(json_doc_get_string(doc, "count", &val) != 0);
    ASSERT(val == NULL);
    json_free(doc);
    return 0;
}

static int test_json_get_string_escaped(void)
{
    JsonDoc* doc = json_parse("{\"msg\": \"hello\\nworld\"}");
    ASSERT(doc != NULL);
    char* val = NULL;
    ASSERT(json_doc_get_string(doc, "msg", &val) == 0);
    ASSERT(val != NULL);
    ASSERT(strcmp(val, "hello\nworld") == 0);
    free(val);
    json_free(doc);
    return 0;
}

static int test_json_get_string_empty_value(void)
{
    JsonDoc* doc = json_parse("{\"key\": \"\"}");
    ASSERT(doc != NULL);
    char* val = NULL;
    ASSERT(json_doc_get_string(doc, "key", &val) == 0);
    ASSERT(val != NULL);
    ASSERT(val[0] == '\0');
    free(val);
    json_free(doc);
    return 0;
}

static int test_json_get_string_whitespace(void)
{
    /* extra whitespace around colon and value */
    JsonDoc* doc = json_parse("{\"k\" :\t\n \"v\"}");
    ASSERT(doc != NULL);
    char* val = NULL;
    ASSERT(json_doc_get_string(doc, "k", &val) == 0);
    ASSERT(val != NULL);
    ASSERT(strcmp(val, "v") == 0);
    free(val);
    json_free(doc);
    return 0;
}

static int test_json_get_string_multiple_keys(void)
{
    JsonDoc* doc = json_parse("{\"a\": \"1\", \"b\": \"2\", \"c\": \"3\"}");
    ASSERT(doc != NULL);
    char* a = NULL;
    char* b = NULL;
    char* c = NULL;
    ASSERT(json_doc_get_string(doc, "a", &a) == 0);
    ASSERT(json_doc_get_string(doc, "b", &b) == 0);
    ASSERT(json_doc_get_string(doc, "c", &c) == 0);
    ASSERT(a && strcmp(a, "1") == 0);
    ASSERT(b && strcmp(b, "2") == 0);
    ASSERT(c && strcmp(c, "3") == 0);
    free(a); free(b); free(c);
    json_free(doc);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Tests: json_doc_get_int                                            */
/* ------------------------------------------------------------------ */

static int test_json_get_int_basic(void)
{
    JsonDoc* doc = json_parse("{\"count\": 42}");
    ASSERT(doc != NULL);
    int val = 0;
    ASSERT(json_doc_get_int(doc, "count", &val) == 0);
    ASSERT(val == 42);
    json_free(doc);
    return 0;
}

static int test_json_get_int_missing_key(void)
{
    JsonDoc* doc = json_parse("{\"count\": 42}");
    ASSERT(doc != NULL);
    int val = -1;
    ASSERT(json_doc_get_int(doc, "missing", &val) != 0);
    ASSERT(val == -1);
    json_free(doc);
    return 0;
}

static int test_json_get_int_negative(void)
{
    JsonDoc* doc = json_parse("{\"val\": -7}");
    ASSERT(doc != NULL);
    int val = 0;
    ASSERT(json_doc_get_int(doc, "val", &val) == 0);
    ASSERT(val == -7);
    json_free(doc);
    return 0;
}

static int test_json_get_int_zero(void)
{
    JsonDoc* doc = json_parse("{\"val\": 0}");
    ASSERT(doc != NULL);
    int val = -1;
    ASSERT(json_doc_get_int(doc, "val", &val) == 0);
    ASSERT(val == 0);
    json_free(doc);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Tests: json_get_string — partial key match                         */
/* ------------------------------------------------------------------ */

static int test_json_get_string_partial_key_match(void)
{
    /* "name" should not match "namex" */
    JsonDoc* doc = json_parse("{\"namex\": \"no\", \"name\": \"yes\"}");
    ASSERT(doc != NULL);
    char* val = NULL;
    ASSERT(json_doc_get_string(doc, "name", &val) == 0);
    ASSERT(val != NULL);
    ASSERT(strcmp(val, "yes") == 0);
    free(val);
    json_free(doc);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Tests: json_doc_get_string — null inputs                           */
/* ------------------------------------------------------------------ */

static int test_json_get_string_null_json(void)
{
    ASSERT(json_parse(NULL) == NULL);
    return 0;
}

static int test_json_get_string_null_key(void)
{
    JsonDoc* doc = json_parse("{\"k\":\"v\"}");
    ASSERT(doc != NULL);
    char* val = NULL;
    ASSERT(json_doc_get_string(doc, NULL, &val) == -1);
    ASSERT(val == NULL);
    json_free(doc);
    return 0;
}

static int test_json_get_string_null_out(void)
{
    JsonDoc* doc = json_parse("{\"k\":\"v\"}");
    ASSERT(doc != NULL);
    ASSERT(json_doc_get_string(doc, "k", NULL) == -1);
    json_free(doc);
    return 0;
}

static int test_json_get_string_invalid_json(void)
{
    ASSERT(json_parse("not json at all") == NULL);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Tests: json_doc_get_int — null inputs                              */
/* ------------------------------------------------------------------ */

static int test_json_get_int_null_json(void)
{
    ASSERT(json_parse(NULL) == NULL);
    return 0;
}

static int test_json_get_int_null_key(void)
{
    JsonDoc* doc = json_parse("{\"k\":1}");
    ASSERT(doc != NULL);
    int val = -1;
    ASSERT(json_doc_get_int(doc, NULL, &val) == -1);
    ASSERT(val == -1);
    json_free(doc);
    return 0;
}

static int test_json_get_int_null_out(void)
{
    JsonDoc* doc = json_parse("{\"k\":1}");
    ASSERT(doc != NULL);
    ASSERT(json_doc_get_int(doc, "k", NULL) == -1);
    json_free(doc);
    return 0;
}

static int test_json_get_int_invalid_json(void)
{
    ASSERT(json_parse("not json") == NULL);
    return 0;
}

static int test_json_get_int_string_value(void)
{
    /* value is a string, not a number */
    JsonDoc* doc = json_parse("{\"k\":\"hello\"}");
    ASSERT(doc != NULL);
    int val = -1;
    ASSERT(json_doc_get_int(doc, "k", &val) == -1);
    ASSERT(val == -1);
    json_free(doc);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Tests: json_foreach_array_item                                     */
/* ------------------------------------------------------------------ */

static int g_foreach_count;
static char g_foreach_last[256];

static void foreach_cb(void* ctx, const char* item_json)
{
    (void)ctx;
    g_foreach_count++;
    snprintf(g_foreach_last, sizeof(g_foreach_last), "%s", item_json);
}

static int test_json_foreach_basic(void)
{
    JsonDoc* doc = json_parse("{\"items\":[{\"id\":1},{\"id\":2},{\"id\":3}]}");
    ASSERT(doc != NULL);
    g_foreach_count = 0;
    json_doc_foreach_array_item(doc, "items", NULL, foreach_cb);
    ASSERT(g_foreach_count == 3);
    json_free(doc);
    return 0;
}

static int test_json_foreach_empty_array(void)
{
    JsonDoc* doc = json_parse("{\"items\":[]}");
    ASSERT(doc != NULL);
    g_foreach_count = 0;
    json_doc_foreach_array_item(doc, "items", NULL, foreach_cb);
    ASSERT(g_foreach_count == 0);
    json_free(doc);
    return 0;
}

static int test_json_foreach_missing_key(void)
{
    JsonDoc* doc = json_parse("{\"other\":[1,2]}");
    ASSERT(doc != NULL);
    g_foreach_count = 0;
    json_doc_foreach_array_item(doc, "items", NULL, foreach_cb);
    ASSERT(g_foreach_count == 0);
    json_free(doc);
    return 0;
}

static int test_json_foreach_null_json(void)
{
    g_foreach_count = 0;
    json_doc_foreach_array_item(NULL, "key", NULL, foreach_cb);
    ASSERT(g_foreach_count == 0);
    return 0;
}

static int test_json_foreach_null_key(void)
{
    JsonDoc* doc = json_parse("{\"k\":[]}");
    ASSERT(doc != NULL);
    g_foreach_count = 0;
    json_doc_foreach_array_item(doc, NULL, NULL, foreach_cb);
    ASSERT(g_foreach_count == 0);
    json_free(doc);
    return 0;
}

static int test_json_foreach_null_cb(void)
{
    JsonDoc* doc = json_parse("{\"k\":[1]}");
    ASSERT(doc != NULL);
    /* Must not crash with NULL callback */
    json_doc_foreach_array_item(doc, "k", NULL, NULL);
    json_free(doc);
    return 0;
}

static int test_json_foreach_not_array(void)
{
    JsonDoc* doc = json_parse("{\"items\":\"not_array\"}");
    ASSERT(doc != NULL);
    g_foreach_count = 0;
    json_doc_foreach_array_item(doc, "items", NULL, foreach_cb);
    ASSERT(g_foreach_count == 0);
    json_free(doc);
    return 0;
}

static int test_json_foreach_single_element(void)
{
    JsonDoc* doc = json_parse("{\"v\":[{\"name\":\"alice\"}]}");
    ASSERT(doc != NULL);
    g_foreach_count = 0;
    memset(g_foreach_last, 0, sizeof(g_foreach_last));
    json_doc_foreach_array_item(doc, "v", NULL, foreach_cb);
    ASSERT(g_foreach_count == 1);
    ASSERT(strstr(g_foreach_last, "alice") != NULL);
    json_free(doc);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Tests: json_parse / json_free                                      */
/* ------------------------------------------------------------------ */

static int test_json_parse_basic(void)
{
    JsonDoc* doc = json_parse("{\"key\":\"val\"}");
    ASSERT(doc != NULL);
    json_free(doc);
    return 0;
}

static int test_json_parse_null(void)
{
    ASSERT(json_parse(NULL) == NULL);
    return 0;
}

static int test_json_parse_invalid(void)
{
    ASSERT(json_parse("not json") == NULL);
    return 0;
}

static int test_json_free_null(void)
{
    json_free(NULL); /* must not crash */
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Tests: json_doc_get_string / json_doc_get_int                      */
/* ------------------------------------------------------------------ */

static int test_json_doc_get_string_basic(void)
{
    JsonDoc* doc = json_parse("{\"name\":\"alice\",\"age\":30}");
    ASSERT(doc != NULL);
    char* val = NULL;
    ASSERT(json_doc_get_string(doc, "name", &val) == 0);
    ASSERT(strcmp(val, "alice") == 0);
    free(val);
    json_free(doc);
    return 0;
}

static int test_json_doc_get_string_missing(void)
{
    JsonDoc* doc = json_parse("{\"name\":\"alice\"}");
    ASSERT(doc != NULL);
    char* val = NULL;
    ASSERT(json_doc_get_string(doc, "missing", &val) == -1);
    ASSERT(val == NULL);
    json_free(doc);
    return 0;
}

static int test_json_doc_get_string_null_doc(void)
{
    char* val = NULL;
    ASSERT(json_doc_get_string(NULL, "key", &val) == -1);
    return 0;
}

static int test_json_doc_get_int_basic(void)
{
    JsonDoc* doc = json_parse("{\"count\":42}");
    ASSERT(doc != NULL);
    int val = 0;
    ASSERT(json_doc_get_int(doc, "count", &val) == 0);
    ASSERT(val == 42);
    json_free(doc);
    return 0;
}

static int test_json_doc_get_int_missing(void)
{
    JsonDoc* doc = json_parse("{\"count\":42}");
    ASSERT(doc != NULL);
    int val = -1;
    ASSERT(json_doc_get_int(doc, "missing", &val) == -1);
    ASSERT(val == -1);
    json_free(doc);
    return 0;
}

static int test_json_doc_get_int_null_doc(void)
{
    int val = 0;
    ASSERT(json_doc_get_int(NULL, "key", &val) == -1);
    return 0;
}

static int test_json_doc_multiple_queries(void)
{
    /* Parse once, query multiple fields — the key optimization. */
    JsonDoc* doc = json_parse(
        "{\"token_type\":\"Bearer\",\"scope\":\"Files.Read\","
        "\"expires_in\":3600,\"access_token\":\"tok\","
        "\"refresh_token\":\"ref\"}");
    ASSERT(doc != NULL);

    char* token_type = NULL;
    char* scope = NULL;
    char* access_token = NULL;
    char* refresh_token = NULL;
    int expires_in = 0;

    ASSERT(json_doc_get_string(doc, "token_type", &token_type) == 0);
    ASSERT(json_doc_get_string(doc, "scope", &scope) == 0);
    ASSERT(json_doc_get_int(doc, "expires_in", &expires_in) == 0);
    ASSERT(json_doc_get_string(doc, "access_token", &access_token) == 0);
    ASSERT(json_doc_get_string(doc, "refresh_token", &refresh_token) == 0);

    ASSERT(strcmp(token_type, "Bearer") == 0);
    ASSERT(strcmp(scope, "Files.Read") == 0);
    ASSERT(expires_in == 3600);
    ASSERT(strcmp(access_token, "tok") == 0);
    ASSERT(strcmp(refresh_token, "ref") == 0);

    free(token_type);
    free(scope);
    free(access_token);
    free(refresh_token);
    json_free(doc);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Tests: json_doc_foreach_array_item                                 */
/* ------------------------------------------------------------------ */

static int test_json_doc_foreach_basic(void)
{
    JsonDoc* doc = json_parse("{\"items\":[{\"id\":1},{\"id\":2}]}");
    ASSERT(doc != NULL);
    g_foreach_count = 0;
    json_doc_foreach_array_item(doc, "items", NULL, foreach_cb);
    ASSERT(g_foreach_count == 2);
    json_free(doc);
    return 0;
}

static int test_json_doc_foreach_null_doc(void)
{
    g_foreach_count = 0;
    json_doc_foreach_array_item(NULL, "key", NULL, foreach_cb);
    ASSERT(g_foreach_count == 0);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  main                                                               */
/* ------------------------------------------------------------------ */

int main(void)
{
    int passed = 0, failed = 0;

    /* json_get_string */
    RUN_TEST(test_json_get_string_basic);
    RUN_TEST(test_json_get_string_missing_key);
    RUN_TEST(test_json_get_string_int_value);
    RUN_TEST(test_json_get_string_escaped);
    RUN_TEST(test_json_get_string_empty_value);
    RUN_TEST(test_json_get_string_whitespace);
    RUN_TEST(test_json_get_string_multiple_keys);

    /* json_get_int */
    RUN_TEST(test_json_get_int_basic);
    RUN_TEST(test_json_get_int_missing_key);
    RUN_TEST(test_json_get_int_negative);
    RUN_TEST(test_json_get_int_zero);

    /* json_get_string — partial key match */
    RUN_TEST(test_json_get_string_partial_key_match);

    /* json_get_string — null/invalid inputs */
    RUN_TEST(test_json_get_string_null_json);
    RUN_TEST(test_json_get_string_null_key);
    RUN_TEST(test_json_get_string_null_out);
    RUN_TEST(test_json_get_string_invalid_json);

    /* json_get_int — null/invalid inputs */
    RUN_TEST(test_json_get_int_null_json);
    RUN_TEST(test_json_get_int_null_key);
    RUN_TEST(test_json_get_int_null_out);
    RUN_TEST(test_json_get_int_invalid_json);
    RUN_TEST(test_json_get_int_string_value);

    /* json_foreach_array_item */
    RUN_TEST(test_json_foreach_basic);
    RUN_TEST(test_json_foreach_empty_array);
    RUN_TEST(test_json_foreach_missing_key);
    RUN_TEST(test_json_foreach_null_json);
    RUN_TEST(test_json_foreach_null_key);
    RUN_TEST(test_json_foreach_null_cb);
    RUN_TEST(test_json_foreach_not_array);
    RUN_TEST(test_json_foreach_single_element);

    /* json_parse / json_free */
    RUN_TEST(test_json_parse_basic);
    RUN_TEST(test_json_parse_null);
    RUN_TEST(test_json_parse_invalid);
    RUN_TEST(test_json_free_null);

    /* json_doc_get_string / json_doc_get_int */
    RUN_TEST(test_json_doc_get_string_basic);
    RUN_TEST(test_json_doc_get_string_missing);
    RUN_TEST(test_json_doc_get_string_null_doc);
    RUN_TEST(test_json_doc_get_int_basic);
    RUN_TEST(test_json_doc_get_int_missing);
    RUN_TEST(test_json_doc_get_int_null_doc);
    RUN_TEST(test_json_doc_multiple_queries);

    /* json_doc_foreach_array_item */
    RUN_TEST(test_json_doc_foreach_basic);
    RUN_TEST(test_json_doc_foreach_null_doc);

    fprintf(stderr, "  %-50s%s\n", "---", "--");
    fprintf(stderr, "  %d passed, %d failed\n\n", passed, failed);

    return failed;
}
