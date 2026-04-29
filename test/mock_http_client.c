/*
 * mock_http_client.c — Mock implementation of http_client.h for testing.
 *
 * Provides the same symbols as http_client.c but without any curl
 * dependency.  Tests queue responses via mock_http_queue_response()
 * and each http_post/http_get call consumes the next one in order.
 */

#include "http_client.h"
#include "mock_http_client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- mock state ---- */

typedef struct {
    long  status;
    char* body;  /* heap-allocated copy, freed on reset */
} MockResponse;

static MockResponse g_responses[MOCK_MAX_RESPONSES];
static int g_response_count;
static int g_response_index;

static char* g_redirect_url;

static int g_post_count;
static int g_get_count;

/* ---- mock control API ---- */

void mock_http_reset(void)
{
    for (int i = 0; i < g_response_count; i++)
    {
        free(g_responses[i].body);
    }

    g_response_count = 0;
    g_response_index = 0;
    g_post_count     = 0;
    g_get_count      = 0;

    free(g_redirect_url);
    g_redirect_url = NULL;
}

void mock_http_queue_response(long status, const char* body)
{
    if (g_response_count >= MOCK_MAX_RESPONSES)
    {
        return;
    }

    g_responses[g_response_count].status = status;
    g_responses[g_response_count].body   = body ? strdup(body) : NULL;
    g_response_count++;
}

void mock_http_set_redirect(const char* url)
{
    free(g_redirect_url);
    g_redirect_url = url ? strdup(url) : NULL;
}

int mock_http_post_count(void) { return g_post_count; }
int mock_http_get_count(void)  { return g_get_count; }

/* ---- consume next queued response ---- */

static long consume_response(Buffer* response)
{
    response->data = NULL;
    response->size = 0;
    response->cap  = 0;

    if (g_response_index >= g_response_count)
    {
        return -1;
    }

    MockResponse* r = &g_responses[g_response_index++];

    if (r->body)
    {
        size_t len = strlen(r->body);
        response->data = malloc(len + 1);
        if (response->data)
        {
            memcpy(response->data, r->body, len + 1);
            response->size = len;
            response->cap  = len + 1;
        }
    }

    return r->status;
}

/* ---- http_client.h implementation (mock) ---- */

struct HttpClient {
    int dummy;
};

HttpClient* http_client_create(void)
{
    HttpClient* c = calloc(1, sizeof(HttpClient));
    return c;
}

void http_client_destroy(HttpClient* client)
{
    free(client);
}

int http_form_add_param(HttpClient* client, const char* key, const char* value, Buffer* out)
{
    (void)client;

    size_t klen = strlen(key);
    size_t vlen = strlen(value);
    size_t need = out->size + klen + 1 + vlen + 2;

    if (need > out->cap)
    {
        size_t new_cap = (need < 256) ? 256 : need * 2;
        char* tmp = realloc(out->data, new_cap);
        if (!tmp)
        {
            return -1;
        }

        out->data = tmp;
        out->cap = new_cap;
    }

    if (out->size > 0)
    {
        out->data[out->size++] = '&';
    }

    memcpy(out->data + out->size, key, klen); out->size += klen;
    out->data[out->size++] = '=';
    memcpy(out->data + out->size, value, vlen); out->size += vlen;
    out->data[out->size] = '\0';
    return 0;
}

long http_post(HttpClient* client, const char* url, const char* body, Buffer* response)
{
    (void)client; (void)url; (void)body;
    g_post_count++;
    return consume_response(response);
}

long http_get(HttpClient* client, const char* url, const char* bearer_token, int follow_redirects, Buffer* response)
{
    (void)client; (void)url; (void)bearer_token; (void)follow_redirects;
    g_get_count++;
    return consume_response(response);
}

int http_client_url_encode(HttpClient* client, const char* str, Buffer* out)
{
    (void)client;

    if (!client || !str || !out)
    {
        return -1;
    }

    /* Simple mock: copy the string as-is (no real percent-encoding) */
    size_t len = strlen(str);
    out->data = malloc(len + 1);
    if (!out->data)
    {
        return -1;
    }

    memcpy(out->data, str, len);
    out->data[len] = '\0';
    out->size = len;
    out->cap  = len + 1;
    return 0;
}

int http_client_get_redirect_url(HttpClient* client, char** out_url)
{
    (void)client;

    if (!client || !out_url)
    {
        return -1;
    }

    if (!g_redirect_url)
    {
        return -1;
    }

    *out_url = strdup(g_redirect_url);
    return *out_url ? 0 : -1;
}

int http_url_get_param(HttpClient* client, const char* url, const char* param, char** out_value)
{
    (void)client;

    if (!url || !param || !out_value)
    {
        return -1;
    }

    *out_value = NULL;

    const char* query = strchr(url, '?');
    if (!query)
    {
        return -1;
    }

    query++; /* skip '?' */
    size_t plen = strlen(param);
    const char* p = strstr(query, param);
    if (p && (p == query || p[-1] == '&') && p[plen] == '=')
    {
        p += plen + 1;
        const char* end = strchr(p, '&');
        if (!end)
        {
            end = p + strlen(p);
        }

        size_t value_len = (size_t)(end - p);
        *out_value = malloc(value_len + 1);
        if (!*out_value)
        {
            return -1;
        }

        memcpy(*out_value, p, value_len);
        (*out_value)[value_len] = '\0';
    }

    return *out_value ? 0 : -1;
}
