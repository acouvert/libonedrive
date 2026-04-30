/*
 * test_onedrive.c — unit tests for onedrive.c
 *
 * Links against mock_http_client.c (no curl dependency).
 * Each test function returns 0 on success, -1 on failure.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "onedrive.h"
#include "http_client.h"
#include "mock_http_client.h"

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

static const char* SCOPES = "Files.Read offline_access";

/* ------------------------------------------------------------------ */
/*  Tests: onedrive_client_create / destroy                            */
/* ------------------------------------------------------------------ */

static int test_create_basic(void)
{
    OneDriveClient* c = onedrive_client_create("consumers", "id", SCOPES);
    ASSERT(c != NULL);
    onedrive_client_destroy(c);
    return 0;
}

static int test_create_null_tenant(void)
{
    OneDriveClient* c = onedrive_client_create(NULL, "id", SCOPES);
    ASSERT(c == NULL);
    return 0;
}

static int test_create_null_client_id(void)
{
    OneDriveClient* c = onedrive_client_create("consumers", NULL, SCOPES);
    ASSERT(c == NULL);
    return 0;
}

static int test_create_null_scopes(void)
{
    OneDriveClient* c = onedrive_client_create("consumers", "id", NULL);
    ASSERT(c == NULL);
    return 0;
}

static int test_destroy_null(void)
{
    onedrive_client_destroy(NULL); /* must not crash */
    return 0;
}

static int test_create_single_scope(void)
{
    OneDriveClient* c = onedrive_client_create("consumers", "id", "Files.Read");
    ASSERT(c != NULL);
    onedrive_client_destroy(c);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Tests: onedrive_auth_init                                          */
/* ------------------------------------------------------------------ */

static int test_auth_init_null_client(void)
{
    /* Must not crash when client is NULL */
    onedrive_auth_init(NULL, NULL, NULL, NULL);
    return 0;
}

static int stub_load_called;
static void* stub_load_ctx_received;

static int stub_keyvault_load(void* ctx, OneDriveKeyVault* kv)
{
    stub_load_called = 1;
    stub_load_ctx_received = ctx;
    kv->access_token  = strdup("tok");
    kv->expire_at     = 2000000000L;
    kv->refresh_token = strdup("ref");
    return 0;
}

static int stub_save_called;

static int stub_keyvault_save(void* ctx, const OneDriveKeyVault* kv)
{
    (void)ctx; (void)kv;
    stub_save_called = 1;
    return 0;
}

static int test_auth_init_calls_load(void)
{
    OneDriveClient* c = onedrive_client_create("consumers", "id", SCOPES);
    ASSERT(c != NULL);
    stub_load_called = 0;
    onedrive_auth_init(c, NULL, stub_keyvault_load, stub_keyvault_save);
    ASSERT(stub_load_called == 1);
    onedrive_client_destroy(c);
    return 0;
}

static int test_auth_init_null_load_cb(void)
{
    OneDriveClient* c = onedrive_client_create("consumers", "id", SCOPES);
    ASSERT(c != NULL);
    stub_load_called = 0;
    /* NULL load callback — must not crash, load must not be called */
    onedrive_auth_init(c, NULL, NULL, stub_keyvault_save);
    ASSERT(stub_load_called == 0);
    onedrive_client_destroy(c);
    return 0;
}

static int test_auth_init_null_callbacks(void)
{
    OneDriveClient* c = onedrive_client_create("consumers", "id", SCOPES);
    ASSERT(c != NULL);
    /* Both callbacks NULL — must not crash */
    onedrive_auth_init(c, NULL, NULL, NULL);
    onedrive_client_destroy(c);
    return 0;
}

static int test_auth_init_ctx_passed_through(void)
{
    OneDriveClient* c = onedrive_client_create("consumers", "id", SCOPES);
    ASSERT(c != NULL);
    int sentinel = 42;
    stub_load_ctx_received = NULL;
    onedrive_auth_init(c, &sentinel, stub_keyvault_load, stub_keyvault_save);
    ASSERT(stub_load_ctx_received == &sentinel);
    onedrive_client_destroy(c);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Tests: onedrive_get_content_url — parameter validation             */
/* ------------------------------------------------------------------ */

static int test_content_url_null_client(void)
{
    char* url = NULL;
    ASSERT(onedrive_get_content_url(NULL, "/path", &url) == -1);
    return 0;
}

static int test_content_url_null_path(void)
{
    OneDriveClient* c = onedrive_client_create("consumers", "id", SCOPES);
    ASSERT(c != NULL);
    char* url = NULL;
    ASSERT(onedrive_get_content_url(c, NULL, &url) == -1);
    onedrive_client_destroy(c);
    return 0;
}

static int test_content_url_null_out(void)
{
    OneDriveClient* c = onedrive_client_create("consumers", "id", SCOPES);
    ASSERT(c != NULL);
    ASSERT(onedrive_get_content_url(c, "/path", NULL) == -1);
    onedrive_client_destroy(c);
    return 0;
}

static int test_content_url_no_tokens(void)
{
    OneDriveClient* c = onedrive_client_create("consumers", "id", SCOPES);
    ASSERT(c != NULL);
    mock_http_reset();
    char* url = NULL;
    /* No auth configured — no tokens available, must fail */
    ASSERT(onedrive_get_content_url(c, "/path", &url) == -1);
    onedrive_client_destroy(c);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Token loader stubs                                                 */
/* ------------------------------------------------------------------ */

static int load_valid_token(void* ctx, OneDriveKeyVault* kv)
{
    (void)ctx;
    kv->access_token  = strdup("valid_access_token");
    kv->expire_at     = 2000000000L; /* far future */
    kv->refresh_token = strdup("valid_refresh_token");
    return 0;
}

static int load_expired_token(void* ctx, OneDriveKeyVault* kv)
{
    (void)ctx;
    kv->access_token  = strdup("expired_access_token");
    kv->expire_at     = 1000000000L; /* far past */
    kv->refresh_token = strdup("old_refresh_token");
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Tests: onedrive_get_content_url — HTTP error paths                 */
/* ------------------------------------------------------------------ */

static int test_content_url_valid_token_http_fails(void)
{
    OneDriveClient* c = onedrive_client_create("consumers", "id", SCOPES);
    ASSERT(c != NULL);
    onedrive_auth_init(c, NULL, load_valid_token, NULL);
    mock_http_reset();
    char* url = NULL;
    /* Token is valid but graph HTTP call will fail — must return -1 */
    ASSERT(onedrive_get_content_url(c, "/Documents/file.txt", &url) == -1);
    onedrive_client_destroy(c);
    return 0;
}

static int test_content_url_expired_token_refresh_fails(void)
{
    OneDriveClient* c = onedrive_client_create("consumers", "id", SCOPES);
    ASSERT(c != NULL);
    onedrive_auth_init(c, NULL, load_expired_token, NULL);
    mock_http_reset();
    char* url = NULL;
    /* Token is expired, refresh POST will fail — must return -1 */
    ASSERT(onedrive_get_content_url(c, "/path", &url) == -1);
    onedrive_client_destroy(c);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Tests: mock HTTP — redirect URL                                    */
/* ------------------------------------------------------------------ */

static int test_redirect_url_with_mock_set(void)
{
    HttpClient* h = http_client_create();
    ASSERT(h != NULL);
    mock_http_reset();
    mock_http_set_redirect("https://example.com/redirect");
    char* out = NULL;
    ASSERT(http_client_get_redirect_url(h, &out) == 0);
    ASSERT(out != NULL);
    ASSERT(strcmp(out, "https://example.com/redirect") == 0);
    free(out);
    http_client_destroy(h);
    return 0;
}

static int test_redirect_url_no_redirect_set(void)
{
    HttpClient* h = http_client_create();
    ASSERT(h != NULL);
    mock_http_reset();
    char* out = NULL;
    ASSERT(http_client_get_redirect_url(h, &out) == -1);
    http_client_destroy(h);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Tests: onedrive_get_content_url — happy / error paths (mock HTTP)  */
/* ------------------------------------------------------------------ */

static int test_content_url_success(void)
{
    OneDriveClient* c = onedrive_client_create("consumers", "id", SCOPES);
    ASSERT(c != NULL);
    onedrive_auth_init(c, NULL, load_valid_token, NULL);

    mock_http_reset();
    /* GET 1: get_content_url_by_id → 302 (redirect) */
    mock_http_queue_response(302, NULL);
    mock_http_set_redirect("https://cdn.example.com/download/file.txt");

    char* url = NULL;
    ASSERT(onedrive_get_content_url(c, "item123", &url) == 0);
    ASSERT(url != NULL);
    ASSERT(strcmp(url, "https://cdn.example.com/download/file.txt") == 0);
    ASSERT(mock_http_get_count() == 1);
    ASSERT(mock_http_post_count() == 0);
    free(url);

    onedrive_client_destroy(c);
    return 0;
}

static int test_content_url_item_not_found(void)
{
    OneDriveClient* c = onedrive_client_create("consumers", "id", SCOPES);
    ASSERT(c != NULL);
    onedrive_auth_init(c, NULL, load_valid_token, NULL);

    mock_http_reset();
    /* GET 1: content request returns 404 */
    mock_http_queue_response(404, "{\"error\":{\"code\":\"itemNotFound\"}}");

    char* url = NULL;
    ASSERT(onedrive_get_content_url(c, "nonexistent_id", &url) == -1);
    ASSERT(mock_http_get_count() == 1);

    onedrive_client_destroy(c);
    return 0;
}

static int test_content_url_content_not_redirect(void)
{
    OneDriveClient* c = onedrive_client_create("consumers", "id", SCOPES);
    ASSERT(c != NULL);
    onedrive_auth_init(c, NULL, load_valid_token, NULL);

    mock_http_reset();
    /* GET 1: content returns 200 instead of 302 */
    mock_http_queue_response(200, "file-body-data");

    char* url = NULL;
    ASSERT(onedrive_get_content_url(c, "item789", &url) == -1);
    ASSERT(mock_http_get_count() == 1);

    onedrive_client_destroy(c);
    return 0;
}

static int test_content_url_expired_refresh_success(void)
{
    OneDriveClient* c = onedrive_client_create("consumers", "id", SCOPES);
    ASSERT(c != NULL);
    onedrive_auth_init(c, NULL, load_expired_token, NULL);

    mock_http_reset();
    /* POST 1: token refresh → 200 with new tokens */
    mock_http_queue_response(200,
        "{\"token_type\":\"Bearer\",\"scope\":\"Files.Read\","
        "\"expires_in\":3600,\"access_token\":\"new_access\","
        "\"refresh_token\":\"new_refresh\"}");
    /* GET 1: content redirect */
    mock_http_queue_response(302, NULL);
    mock_http_set_redirect("https://cdn.example.com/refreshed.txt");

    char* url = NULL;
    ASSERT(onedrive_get_content_url(c, "item456", &url) == 0);
    ASSERT(url != NULL);
    ASSERT(strcmp(url, "https://cdn.example.com/refreshed.txt") == 0);
    ASSERT(mock_http_post_count() == 1);
    ASSERT(mock_http_get_count() == 1);
    free(url);

    onedrive_client_destroy(c);
    return 0;
}

static int test_content_url_expired_refresh_bad_status(void)
{
    OneDriveClient* c = onedrive_client_create("consumers", "id", SCOPES);
    ASSERT(c != NULL);
    onedrive_auth_init(c, NULL, load_expired_token, NULL);

    mock_http_reset();
    /* POST 1: token refresh returns server error */
    mock_http_queue_response(500, "{\"error\":\"server_error\"}");

    char* url = NULL;
    ASSERT(onedrive_get_content_url(c, "item_err", &url) == -1);

    onedrive_client_destroy(c);
    return 0;
}

static int save_called_flag;

static int save_cb_tracker(void* ctx, const OneDriveKeyVault* kv)
{
    (void)ctx; (void)kv;
    save_called_flag = 1;
    return 0;
}

static int test_content_url_save_called_on_refresh(void)
{
    OneDriveClient* c = onedrive_client_create("consumers", "id", SCOPES);
    ASSERT(c != NULL);
    onedrive_auth_init(c, NULL, load_expired_token, save_cb_tracker);

    save_called_flag = 0;
    mock_http_reset();
    /* POST 1: refresh succeeds */
    mock_http_queue_response(200,
        "{\"token_type\":\"Bearer\",\"scope\":\"Files.Read\","
        "\"expires_in\":3600,\"access_token\":\"saved_tok\","
        "\"refresh_token\":\"saved_ref\"}");
    /* GET 1: content redirect */
    mock_http_queue_response(302, NULL);
    mock_http_set_redirect("https://cdn.example.com/saved.txt");

    char* url = NULL;
    ASSERT(onedrive_get_content_url(c, "item_s", &url) == 0);
    /* save callback must have been invoked during token refresh */
    ASSERT(save_called_flag == 1);
    free(url);

    onedrive_client_destroy(c);
    return 0;
}

static int test_content_url_item_lookup_server_error(void)
{
    OneDriveClient* c = onedrive_client_create("consumers", "id", SCOPES);
    ASSERT(c != NULL);
    onedrive_auth_init(c, NULL, load_valid_token, NULL);

    mock_http_reset();
    /* GET 1: content request returns 500 */
    mock_http_queue_response(500, "{\"error\":\"internalServerError\"}");

    char* url = NULL;
    ASSERT(onedrive_get_content_url(c, "item_500", &url) == -1);

    onedrive_client_destroy(c);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Tests: onedrive_get_item_id                                        */
/* ------------------------------------------------------------------ */

static int test_get_item_id_null_client(void)
{
    char* id = NULL;
    ASSERT(onedrive_get_item_id(NULL, "/path", &id) == -1);
    return 0;
}

static int test_get_item_id_null_path(void)
{
    OneDriveClient* c = onedrive_client_create("consumers", "id", SCOPES);
    ASSERT(c != NULL);
    char* id = NULL;
    ASSERT(onedrive_get_item_id(c, NULL, &id) == -1);
    onedrive_client_destroy(c);
    return 0;
}

static int test_get_item_id_null_out(void)
{
    OneDriveClient* c = onedrive_client_create("consumers", "id", SCOPES);
    ASSERT(c != NULL);
    ASSERT(onedrive_get_item_id(c, "/path", NULL) == -1);
    onedrive_client_destroy(c);
    return 0;
}

static int test_get_item_id_no_tokens(void)
{
    OneDriveClient* c = onedrive_client_create("consumers", "id", SCOPES);
    ASSERT(c != NULL);
    mock_http_reset();
    char* id = NULL;
    ASSERT(onedrive_get_item_id(c, "/Documents/", &id) == -1);
    onedrive_client_destroy(c);
    return 0;
}

static int test_get_item_id_success(void)
{
    OneDriveClient* c = onedrive_client_create("consumers", "id", SCOPES);
    ASSERT(c != NULL);
    onedrive_auth_init(c, NULL, load_valid_token, NULL);

    mock_http_reset();
    mock_http_queue_response(200, "{\"id\": \"ABC123\"}");

    char* id = NULL;
    ASSERT(onedrive_get_item_id(c, "/Documents/", &id) == 0);
    ASSERT(id != NULL);
    ASSERT(strcmp(id, "ABC123") == 0);
    free(id);

    onedrive_client_destroy(c);
    return 0;
}

static int test_get_item_id_not_found(void)
{
    OneDriveClient* c = onedrive_client_create("consumers", "id", SCOPES);
    ASSERT(c != NULL);
    onedrive_auth_init(c, NULL, load_valid_token, NULL);

    mock_http_reset();
    mock_http_queue_response(404, "{\"error\":{\"code\":\"itemNotFound\"}}");

    char* id = NULL;
    ASSERT(onedrive_get_item_id(c, "/nonexistent/", &id) == -1);

    onedrive_client_destroy(c);
    return 0;
}

static int test_get_item_id_server_error(void)
{
    OneDriveClient* c = onedrive_client_create("consumers", "id", SCOPES);
    ASSERT(c != NULL);
    onedrive_auth_init(c, NULL, load_valid_token, NULL);

    mock_http_reset();
    mock_http_queue_response(500, "{\"error\":\"internalServerError\"}");

    char* id = NULL;
    ASSERT(onedrive_get_item_id(c, "/path/", &id) == -1);

    onedrive_client_destroy(c);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Tests: onedrive_get_delta                                          */
/* ------------------------------------------------------------------ */

static int test_get_delta_null_client(void)
{
    char* dt = NULL;
    ASSERT(onedrive_get_delta(NULL, "id", NULL, NULL, NULL, NULL, &dt) == -1);
    return 0;
}

static int test_get_delta_null_item_id(void)
{
    OneDriveClient* c = onedrive_client_create("consumers", "id", SCOPES);
    ASSERT(c != NULL);
    char* dt = NULL;
    ASSERT(onedrive_get_delta(c, NULL, NULL, NULL, NULL, NULL, &dt) == -1);
    onedrive_client_destroy(c);
    return 0;
}

static int test_get_delta_null_token(void)
{
    OneDriveClient* c = onedrive_client_create("consumers", "id", SCOPES);
    ASSERT(c != NULL);
    ASSERT(onedrive_get_delta(c, "id", NULL, NULL, NULL, NULL, NULL) == -1);
    onedrive_client_destroy(c);
    return 0;
}

static int delta_item_count;

static void delta_item_counter(void* ctx, OneDriveDeltaAction action, const char* item_json)
{
    (void)ctx; (void)action; (void)item_json;
    delta_item_count++;
}

static int test_get_delta_success(void)
{
    OneDriveClient* c = onedrive_client_create("consumers", "id", SCOPES);
    ASSERT(c != NULL);
    onedrive_auth_init(c, NULL, load_valid_token, NULL);

    mock_http_reset();
    /* GET 1: delta response with deltaLink */
    mock_http_queue_response(200,
        "{\"value\":[{\"id\":\"1\"},{\"id\":\"2\"}],"
        "\"@odata.deltaLink\":\"https://graph.microsoft.com/delta?token=abc123\"}");

    delta_item_count = 0;
    char* dt = NULL;
    ASSERT(onedrive_get_delta(c, "item_id", "id,name", NULL, NULL, delta_item_counter, &dt) == 0);
    ASSERT(dt != NULL);
    ASSERT(strcmp(dt, "abc123") == 0);
    ASSERT(delta_item_count == 2);
    free(dt);

    onedrive_client_destroy(c);
    return 0;
}

static int test_get_delta_pagination(void)
{
    OneDriveClient* c = onedrive_client_create("consumers", "id", SCOPES);
    ASSERT(c != NULL);
    onedrive_auth_init(c, NULL, load_valid_token, NULL);

    mock_http_reset();
    /* GET 1: first page with nextLink */
    mock_http_queue_response(200,
        "{\"value\":[{\"id\":\"1\"}],"
        "\"@odata.nextLink\":\"https://graph.microsoft.com/delta?skiptoken=page2\"}");
    /* GET 2: second page with deltaLink */
    mock_http_queue_response(200,
        "{\"value\":[{\"id\":\"2\"}],"
        "\"@odata.deltaLink\":\"https://graph.microsoft.com/delta?token=final_tok\"}");

    delta_item_count = 0;
    char* dt = NULL;
    ASSERT(onedrive_get_delta(c, "item_id", "id", NULL, NULL, delta_item_counter, &dt) == 0);
    ASSERT(dt != NULL);
    ASSERT(strcmp(dt, "final_tok") == 0);
    ASSERT(delta_item_count == 2);
    ASSERT(mock_http_get_count() == 2);
    free(dt);

    onedrive_client_destroy(c);
    return 0;
}

static int test_get_delta_no_tokens(void)
{
    OneDriveClient* c = onedrive_client_create("consumers", "id", SCOPES);
    ASSERT(c != NULL);
    mock_http_reset();
    char* dt = NULL;
    /* No auth — must fail */
    ASSERT(onedrive_get_delta(c, "item_id", "id", NULL, NULL, delta_item_counter, &dt) == -1);
    onedrive_client_destroy(c);
    return 0;
}

static int test_get_delta_server_error(void)
{
    OneDriveClient* c = onedrive_client_create("consumers", "id", SCOPES);
    ASSERT(c != NULL);
    onedrive_auth_init(c, NULL, load_valid_token, NULL);

    mock_http_reset();
    mock_http_queue_response(500, "{\"error\":\"server_error\"}");

    char* dt = NULL;
    ASSERT(onedrive_get_delta(c, "item_id", "id", NULL, NULL, delta_item_counter, &dt) == -1);

    onedrive_client_destroy(c);
    return 0;
}

static int test_get_delta_null_callback(void)
{
    OneDriveClient* c = onedrive_client_create("consumers", "id", SCOPES);
    ASSERT(c != NULL);
    onedrive_auth_init(c, NULL, load_valid_token, NULL);

    mock_http_reset();
    mock_http_queue_response(200,
        "{\"value\":[{\"id\":\"1\"}],"
        "\"@odata.deltaLink\":\"https://graph.microsoft.com/delta?token=tok1\"}");

    char* dt = NULL;
    /* NULL callback — should still succeed and extract token */
    ASSERT(onedrive_get_delta(c, "item_id", "id", NULL, NULL, NULL, &dt) == 0);
    ASSERT(dt != NULL);
    ASSERT(strcmp(dt, "tok1") == 0);
    free(dt);

    onedrive_client_destroy(c);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  main                                                               */
/* ------------------------------------------------------------------ */

int main(void)
{
    int passed = 0, failed = 0;

    /* onedrive_client_create / destroy */
    RUN_TEST(test_create_basic);
    RUN_TEST(test_create_null_tenant);
    RUN_TEST(test_create_null_client_id);
    RUN_TEST(test_create_null_scopes);
    RUN_TEST(test_destroy_null);
    RUN_TEST(test_create_single_scope);

    /* onedrive_auth_init */
    RUN_TEST(test_auth_init_null_client);
    RUN_TEST(test_auth_init_calls_load);
    RUN_TEST(test_auth_init_null_load_cb);
    RUN_TEST(test_auth_init_null_callbacks);
    RUN_TEST(test_auth_init_ctx_passed_through);

    /* onedrive_get_content_url — parameter validation */
    RUN_TEST(test_content_url_null_client);
    RUN_TEST(test_content_url_null_path);
    RUN_TEST(test_content_url_null_out);
    RUN_TEST(test_content_url_no_tokens);
    RUN_TEST(test_content_url_valid_token_http_fails);
    RUN_TEST(test_content_url_expired_token_refresh_fails);

    /* mock HTTP — redirect URL */
    RUN_TEST(test_redirect_url_with_mock_set);
    RUN_TEST(test_redirect_url_no_redirect_set);

    /* onedrive_get_content_url — mock HTTP paths */
    RUN_TEST(test_content_url_success);
    RUN_TEST(test_content_url_item_not_found);
    RUN_TEST(test_content_url_content_not_redirect);
    RUN_TEST(test_content_url_expired_refresh_success);
    RUN_TEST(test_content_url_expired_refresh_bad_status);
    RUN_TEST(test_content_url_save_called_on_refresh);
    RUN_TEST(test_content_url_item_lookup_server_error);

    /* onedrive_get_item_id */
    RUN_TEST(test_get_item_id_null_client);
    RUN_TEST(test_get_item_id_null_path);
    RUN_TEST(test_get_item_id_null_out);
    RUN_TEST(test_get_item_id_no_tokens);
    RUN_TEST(test_get_item_id_success);
    RUN_TEST(test_get_item_id_not_found);
    RUN_TEST(test_get_item_id_server_error);

    /* onedrive_get_delta */
    RUN_TEST(test_get_delta_null_client);
    RUN_TEST(test_get_delta_null_item_id);
    RUN_TEST(test_get_delta_null_token);
    RUN_TEST(test_get_delta_success);
    RUN_TEST(test_get_delta_pagination);
    RUN_TEST(test_get_delta_no_tokens);
    RUN_TEST(test_get_delta_server_error);
    RUN_TEST(test_get_delta_null_callback);

    fprintf(stderr, "  %-50s%s\n", "---", "--");
    fprintf(stderr, "  %d passed, %d failed\n\n", passed, failed);

    return failed;
}
