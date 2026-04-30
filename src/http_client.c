#include "http_client.h"

#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct HttpClient {
    CURL* curl;
};

static size_t write_cb(void* ptr, size_t size, size_t nmemb, void* userdata)
{
    size_t total = size * nmemb;
    Buffer* buf = (Buffer*)userdata;

    if (buffer_append(buf, (const char*)ptr, total) != 0)
    {
        return 0;
    }

    return total;
}

static int g_curl_ref_count = 0;

HttpClient* http_client_create(void)
{
    if (g_curl_ref_count == 0)
    {
        if (curl_global_init(CURL_GLOBAL_DEFAULT) != 0)
        {
            return NULL;
        }
    }

    g_curl_ref_count++;

    HttpClient* client = calloc(1, sizeof(HttpClient));
    if (!client)
    {
        http_client_destroy(client);
        return NULL;
    }

    client->curl = curl_easy_init();
    if (!client->curl)
    {
        http_client_destroy(client);
        return NULL;
    }

    return client;
}

void http_client_destroy(HttpClient* client)
{
    if (!client)
    {
        return;
    }

    if (client->curl)
    {
        curl_easy_cleanup(client->curl);
    }

    free(client);

    g_curl_ref_count--;
    if (g_curl_ref_count == 0)
    {
        curl_global_cleanup();
    }
}

int http_form_add_param(HttpClient* client, const char* key, const char* value, Buffer* out)
{
    char* esc = curl_easy_escape(client->curl, value, strlen(value));
    if (!esc)
    {
        return -1;
    }

    size_t klen = strlen(key);
    size_t vlen = strlen(esc);

    if (out->size > 0)
    {
        buffer_append(out, "&", 1);
    }

    buffer_append(out, key, klen);
    buffer_append(out, "=", 1);
    buffer_append(out, esc, vlen);

    curl_free(esc);
    return 0;
}

long http_post(HttpClient* client, const char* url, const char* body, Buffer* out_response)
{
    CURL* curl = client->curl;
    curl_easy_reset(curl);

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
    if (!headers)
    {
        return -1;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, out_response);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);

    if (res != CURLE_OK)
    {
        buffer_free(out_response);
        return -1;
    }

    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    return status;
}

long http_get(HttpClient* client, const char* url, const char* bearer_token, int follow_redirects, Buffer* out_response)
{
    CURL* curl = client->curl;
    curl_easy_reset(curl);

    char auth_header[22 + strlen(bearer_token) + 1]; /* "Authorization: bearer " + token */
    snprintf(auth_header, sizeof(auth_header), "Authorization: bearer %s", bearer_token);

    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, auth_header);
    if (!headers)
    {
        return -1;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, out_response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, follow_redirects ? 1L : 0L);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);

    if (res != CURLE_OK)
    {
        buffer_free(out_response);
        return -1;
    }

    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    return status;
}

int http_client_url_encode(HttpClient* client, const char* str, Buffer* out)
{
    if (!client || !str || !out)
    {
        return -1;
    }

    char* encoded = curl_easy_escape(client->curl, str, strlen(str));
    if (!encoded)
    {
        return -1;
    }

    int ret = buffer_append(out, encoded, strlen(encoded));
    curl_free(encoded);
    return ret;
}

int http_client_get_redirect_url(HttpClient* client, char** out_url)
{
    if (!client || !out_url || *out_url) /* out_url must point to NULL */
    {
        return -1;
    }

    char* redirect_url = NULL;
    if (curl_easy_getinfo(client->curl, CURLINFO_REDIRECT_URL, &redirect_url) != CURLE_OK
        || !redirect_url)
    {
        return -1;
    }

    /* redirect_url must not be freed, as it refers to memory managed internally by libcurl. */
    *out_url = strdup(redirect_url);
    return *out_url ? 0 : -1;
}

int http_url_get_param(HttpClient* client, const char* url, const char* param, char** out_value)
{
    (void)client;

    if (!url || !param || !out_value || *out_value) /* out_value must point to NULL */
    {
        return -1;
    }

    char* value = NULL;
    CURLU* curlu = curl_url();

    if (curlu
        && curl_url_set(curlu, CURLUPART_URL, url, 0) == CURLUE_OK
        && curl_url_get(curlu, CURLUPART_QUERY, &value, 0) == CURLUE_OK
        && value)
    {
        size_t plen = strlen(param);
        const char* s = value;

        while ((s = strstr(s, param)) != NULL)
        {
            if ((s == value || s[-1] == '&') && s[plen] == '=')
            {
                const char* vstart = s + plen + 1;
                const char* vend = strchr(vstart, '&');
                if (!vend)
                {
                    vend = vstart + strlen(vstart);
                }

                size_t value_len = (size_t)(vend - vstart);
                *out_value = malloc(value_len + 1);
                if (!*out_value)
                {
                    break;
                }

                memcpy(*out_value, vstart, value_len);
                (*out_value)[value_len] = '\0';
                break;
            }

            s += plen;
        }

        curl_free(value);
    }

    curl_url_cleanup(curlu);
    return *out_value ? 0 : -1;
}

long http_get_range(
    HttpClient* client,
    const char* url,
    size_t from,
    size_t to,
    Buffer* out_response)
{
    CURL* curl = client->curl;
    curl_easy_reset(curl);

    char range[64];
    snprintf(range, sizeof(range), "%zu-%zu", from, to);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_RANGE, range);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, out_response);

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK)
    {
        return -1;
    }

    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    return status;
}
