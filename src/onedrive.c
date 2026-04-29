#include "onedrive.h"
#include "buffer.h"
#include "json_utils.h"
#include "http_client.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    void* onedrive_keyvault_ctx;
    onedrive_keyvault_load_cb on_keyvault_load;
    onedrive_keyvault_save_cb on_keyvault_save;
    OneDriveKeyVault kv;
} OneDriveAuth;

struct OneDriveClient {
    char* tenant;
    char* client_id;
    char* scope;
    HttpClient* http;
    OneDriveAuth auth;
};

typedef struct {
    char* device_code;
    char* user_code;
    char* verification_uri;
    int   expires_in;
    int   interval;
    char* message;
} DeviceAuthInfo;

typedef struct {
    char* token_type;
    char* scope;
    int   expires_in;
    char* access_token;
    char* refresh_token;
} TokenInfo;

static int build_login_url(
    const OneDriveClient* client,
    const char* path,
    Buffer* out_url)
{
    return buffer_printf(out_url, "https://login.microsoftonline.com/%s/oauth2/v2.0/%s", client->tenant, path);
}

static int build_graph_get_item_id_from_path_url(
    const OneDriveClient* client,
    const char* item_path,
    Buffer* out_url)
{
    /* Encode each path segment individually, preserving literal '/' separators. */
    Buffer encoded = {0};
    const char* p = item_path;

    while (*p)
    {
        if (*p == '/')
        {
            if (buffer_append(&encoded, "/", 1) != 0)
            {
                buffer_free(&encoded);
                return -1;
            }

            p++;
            continue;
        }

        const char* seg_start = p;
        while (*p && *p != '/')
        {
            p++;
        }

        size_t seg_len = (size_t)(p - seg_start);
        Buffer seg_copy = {0};
        if (buffer_append(&seg_copy, seg_start, seg_len) != 0)
        {
            buffer_free(&encoded);
            return -1;
        }

        Buffer seg_enc = {0};
        int rc = http_client_url_encode(client->http, seg_copy.data, &seg_enc);
        buffer_free(&seg_copy);

        if (rc != 0)
        {
            buffer_free(&encoded);
            return -1;
        }

        rc = buffer_append(&encoded, seg_enc.data, seg_enc.size);
        buffer_free(&seg_enc);

        if (rc != 0)
        {
            buffer_free(&encoded);
            return -1;
        }
    }

    int ret = buffer_printf(out_url, "https://graph.microsoft.com/v1.0/me/drive/root:%s?select=id", encoded.data);
    buffer_free(&encoded);
    return ret;
}

static int build_graph_get_content_url_by_id_url(
    const OneDriveClient* client,
    const char* item_id,
    Buffer* out_url)
{
    (void)client;
    return buffer_printf(out_url, "https://graph.microsoft.com/v1.0/me/drive/items/%s/content", item_id);
}

static int build_graph_get_delta_url(
    const OneDriveClient* client,
    const char* item_id,
    const char* select,
    const char* delta_token,
    Buffer* out_url)
{
    (void)client;
    const char* select_param = select ? select : "";
    const char* delta_token_param = delta_token ? delta_token : "";
    return buffer_printf(out_url, "https://graph.microsoft.com/v1.0/me/drive/items/%s/delta?select=%s&token=%s", item_id, select_param, delta_token_param);
}

static void device_auth_info_free(DeviceAuthInfo* info)
{
    assert(info);

    free(info->device_code);
    free(info->user_code);
    free(info->verification_uri);
    free(info->message);

    info->expires_in = 0;
    info->interval = 0;
    info->device_code = NULL;
    info->user_code = NULL;
    info->verification_uri = NULL;
    info->message = NULL;
}

static void token_info_free(TokenInfo* info)
{
    assert(info);

    free(info->token_type);
    free(info->scope);
    free(info->access_token);
    free(info->refresh_token);

    info->expires_in = 0;
    info->token_type = NULL;
    info->scope = NULL;
    info->access_token = NULL;
    info->refresh_token = NULL;
}

static int parse_device_auth_response(const char* data, DeviceAuthInfo* out_info)
{
    if (!data || !out_info)
    {
        return -1;
    }

    device_auth_info_free(out_info);

    JsonDoc* doc = json_parse(data);
    if (!doc)
    {
        return -1;
    }

    if (json_doc_get_string(doc, "device_code", &out_info->device_code) != 0
        || json_doc_get_string(doc, "user_code", &out_info->user_code) != 0
        || json_doc_get_string(doc, "verification_uri", &out_info->verification_uri) != 0
        || json_doc_get_int(doc, "expires_in", &out_info->expires_in) != 0
        || json_doc_get_int(doc, "interval", &out_info->interval) != 0
        || json_doc_get_string(doc, "message", &out_info->message) != 0)
    {
        device_auth_info_free(out_info);
        json_free(doc);
        return -1;
    }

    json_free(doc);
    return 0;
}

static int get_device_authorization_info(OneDriveClient* client, DeviceAuthInfo* out_info)
{
    Buffer url = {0};
    Buffer form = {0};
    Buffer resp = {0};

    if (build_login_url(client, "devicecode", &url) != 0
        || http_form_add_param(client->http, "client_id", client->client_id, &form) != 0
        || http_form_add_param(client->http, "scope", client->scope, &form) != 0)
    {
        buffer_free(&url);
        buffer_free(&form);
        return -1;
    }

    long status = http_post(client->http, url.data, form.data, &resp);

    if (status == 200)
    {
        if (parse_device_auth_response(resp.data, out_info) != 0)
        {
            status = -1;
        }
    }

    buffer_free(&url);
    buffer_free(&form);
    buffer_free(&resp);

    return status == 200 ? 0 : -1;
}

static int parse_token_response(const char* data, TokenInfo* out_info)
{
    if (!data || !out_info)
    {
        return -1;
    }

    token_info_free(out_info);

    JsonDoc* doc = json_parse(data);
    if (!doc)
    {
        return -1;
    }

    if (json_doc_get_string(doc, "token_type", &out_info->token_type) != 0
        || json_doc_get_string(doc, "scope", &out_info->scope) != 0
        || json_doc_get_int(doc, "expires_in", &out_info->expires_in) != 0
        || json_doc_get_string(doc, "access_token", &out_info->access_token) != 0
        || json_doc_get_string(doc, "refresh_token", &out_info->refresh_token) != 0)
    {
        token_info_free(out_info);
        json_free(doc);
        return -1;
    }

    json_free(doc);
    return 0;
}

static int request_token(
    OneDriveClient* client,
    const char* grant_type,
    const char* param_key,
    const char* param_value,
    TokenInfo* out_info)
{
    Buffer url = {0};
    Buffer form = {0};
    Buffer resp = {0};

    if (build_login_url(client, "token", &url) != 0
        || http_form_add_param(client->http, "client_id", client->client_id, &form) != 0
        || http_form_add_param(client->http, "grant_type", grant_type, &form) != 0
        || http_form_add_param(client->http, param_key, param_value, &form) != 0)
    {
        buffer_free(&url);
        buffer_free(&form);
        return -1;
    }

    long status = http_post(client->http, url.data, form.data, &resp);

    if (status == 200)
    {
        if (parse_token_response(resp.data, out_info) != 0)
        {
            status = -1;
        }
    }

    buffer_free(&url);
    buffer_free(&form);
    buffer_free(&resp);

    return status == 200 ? 0 : -1;
}

static void keyvault_update(OneDriveClient* client, TokenInfo* info, long now)
{
    OneDriveKeyVault* kv = &client->auth.kv;

    free(kv->access_token);
    free(kv->refresh_token);

    kv->access_token  = info->access_token;
    kv->refresh_token = info->refresh_token;
    kv->expire_at     = now + info->expires_in;

    info->access_token  = NULL;
    info->refresh_token = NULL;

    if (client->auth.on_keyvault_save)
    {
        client->auth.on_keyvault_save(client->auth.onedrive_keyvault_ctx, &client->auth.kv);
    }
}

static char* get_access_token(OneDriveClient* client)
{
    OneDriveKeyVault* kv = &client->auth.kv;
    long now = (long)time(NULL);

    /* Keyvault is populated: validate or refresh */
    if (kv->access_token && kv->refresh_token)
    {
        if (kv->expire_at > now)
        {
            return strdup(kv->access_token);
        }

        TokenInfo token_info = {0};
        if (request_token(client, "refresh_token", "refresh_token", kv->refresh_token, &token_info) != 0)
        {
            return NULL;
        }

        keyvault_update(client, &token_info, now);
        char* token = strdup(kv->access_token);
        token_info_free(&token_info);
        return token;
    }

    /* Keyvault is empty: perform device authorization flow */
    DeviceAuthInfo device_auth_info = {0};
    if (get_device_authorization_info(client, &device_auth_info) != 0)
    {
        return NULL;
    }

    if (device_auth_info.message)
    {
        printf("%s\n", device_auth_info.message);
    }

    long expire_at = now + device_auth_info.expires_in;
    int  interval  = (device_auth_info.interval > 0) ? device_auth_info.interval : 5;

    char* device_code = strdup(device_auth_info.device_code);
    device_auth_info_free(&device_auth_info);

    if (!device_code)
    {
        return NULL;
    }

    char* result = NULL;

    while ((long)time(NULL) < expire_at)
    {
        sleep((unsigned int)interval);

        TokenInfo token_info = {0};
        if (request_token(client, "urn:ietf:params:oauth:grant-type:device_code", "device_code", device_code, &token_info) == 0)
        {
            keyvault_update(client, &token_info, (long)time(NULL));
            result = strdup(kv->access_token);
            token_info_free(&token_info);
            break;
        }
    }

    free(device_code);
    return result;
}

OneDriveClient* onedrive_client_create(const char* tenant, const char* client_id, const char* scopes)
{
    if (!tenant || !client_id || !scopes)
    {
        return NULL;
    }

    OneDriveClient* client = calloc(1, sizeof(OneDriveClient));
    if (!client)
    {
        return NULL;
    }

    client->tenant    = strdup(tenant);
    client->client_id = strdup(client_id);
    client->scope     = strdup(scopes);
    client->http      = http_client_create();

    if (!client->tenant || !client->client_id || !client->scope || !client->http)
    {
        onedrive_client_destroy(client);
        return NULL;
    }

    return client;
}

void onedrive_client_destroy(OneDriveClient* client)
{
    if (!client)
    {
        return;
    }

    free(client->tenant);
    free(client->client_id);
    free(client->scope);
    free(client->auth.kv.access_token);
    free(client->auth.kv.refresh_token);

    http_client_destroy(client->http);

    free(client);
}

void onedrive_auth_init(
    OneDriveClient* client,
    void* onedrive_keyvault_ctx,
    onedrive_keyvault_load_cb on_keyvault_load,
    onedrive_keyvault_save_cb on_keyvault_save)
{
    if (!client)
    {
        return;
    }

    client->auth.onedrive_keyvault_ctx = onedrive_keyvault_ctx;
    client->auth.on_keyvault_load      = on_keyvault_load;
    client->auth.on_keyvault_save      = on_keyvault_save;

    if (on_keyvault_load)
    {
        on_keyvault_load(onedrive_keyvault_ctx, &client->auth.kv);
    }
}

int onedrive_get_item_id(OneDriveClient* client, const char* item_path, char** out_item_id)
{
    if (!client || !item_path || !out_item_id || *out_item_id) /* out_item_id must point to NULL */
    {
        return -1;
    }

    long status = -1;
    Buffer url = {0};
    Buffer resp = {0};

    char* access_token = get_access_token(client);
    if (!access_token)
    {
        return -1;
    }

    if (build_graph_get_item_id_from_path_url(client, item_path, &url) == 0)
    {
        status = http_get(client->http, url.data, access_token, /* follow_redirects */ 1, &resp);
        if (status == 200)
        {
            JsonDoc* doc = json_parse(resp.data);
            if (doc)
            {
                json_doc_get_string(doc, "id", out_item_id);
                json_free(doc);
            }
        }
    }

    free(access_token);
    buffer_free(&url);
    buffer_free(&resp);

    return (status == 200 && *out_item_id) ? 0 : -1;
}

int onedrive_get_content_url(OneDriveClient* client, const char* item_id, char** out_url)
{
    if (!client || !item_id || !out_url || *out_url) /* out_url must point to NULL */
    {
        return -1;
    }

    long status = -1;
    Buffer url = {0};
    Buffer resp = {0};

    char* access_token = get_access_token(client);
    if (!access_token)
    {
        return -1;
    }

    if (build_graph_get_content_url_by_id_url(client, item_id, &url) == 0)
    {
        status = http_get(client->http, url.data, access_token, /* follow_redirects  */0, &resp);
        if (status == 302)
        {
            if (http_client_get_redirect_url(client->http, out_url) != 0)
            {
                status = -1;
            }
        }
    }

    free(access_token);
    buffer_free(&url);
    buffer_free(&resp);

    return status == 302 ? 0 : -1;
}

int onedrive_get_delta(
    OneDriveClient* client,
    const char* item_id,
    const char* select,
    const char* delta_token,
    void* cb_ctx,
    onedrive_delta_item_cb on_item_changed,
    char** out_delta_token)
{
    if (!client || !item_id || !out_delta_token)
    {
        return -1;
    }

    Buffer url = {0};
    Buffer resp = {0};

    if (build_graph_get_delta_url(client, item_id, select, delta_token, &url) != 0)
    {
        buffer_free(&url);
        return -1;
    }

    /* The URL is built — safe to free *out_delta_token now, even if the caller
     * passed the same pointer as both delta_token and *out_delta_token. */
    free(*out_delta_token);
    *out_delta_token = NULL;

    while(url.size > 0)
    {
        char* access_token = get_access_token(client);
        if (!access_token)
        {
            break;
        }

        buffer_reset(&resp);

        long status = http_get(client->http, url.data, access_token, /* follow_redirects */ 1, &resp);

        free(access_token);
        buffer_reset(&url);

        if (status == 200)
        {
            JsonDoc* doc = json_parse(resp.data);
            if (!doc)
            {
                break;
            }

            /* Invoke the callback for each item in the "value" array. */
            if (on_item_changed)
            {
                json_doc_foreach_array_item(doc, "value", cb_ctx, (json_array_item_cb)on_item_changed);
            }

            /* Follow pagination or finish. */
            char* link = NULL;
            if (json_doc_get_string(doc, "@odata.nextLink", &link) == 0)
            {
                buffer_append(&url, link, strlen(link));
            }
            else if (json_doc_get_string(doc, "@odata.deltaLink", &link) == 0)
            {
                http_url_get_param(client->http, link, "token", out_delta_token);
            }

            free(link);
            json_free(doc);
        }
    }

    buffer_free(&url);
    buffer_free(&resp);

    return *out_delta_token ? 0 : -1;
}
