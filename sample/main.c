#include "onedrive.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TENANT    "consumers"
#define CLIENT_ID ""

static const char* SCOPES = "Files.Read offline_access";

static const char* TOKEN_FILE = "tokens.dat";

static int on_keyvault_load(void* ctx, OneDriveKeyVault* kv)
{
    (void)ctx;

    FILE* f = fopen(TOKEN_FILE, "r");
    if (!f)
    {
        return -1;
    }

    char access_token[4096];
    char refresh_token[4096];
    long expire_at;

    if (fscanf(f, "%4095s %ld %4095s", access_token, &expire_at, refresh_token) != 3)
    {
        fclose(f);
        return -1;
    }

    fclose(f);

    kv->access_token = strdup(access_token);
    kv->expire_at = expire_at;
    kv->refresh_token = strdup(refresh_token);
    return 0;
}

static int on_keyvault_save(void* ctx, const OneDriveKeyVault* kv)
{
    (void)ctx;

    FILE* f = fopen(TOKEN_FILE, "w");
    if (!f)
    {
        return -1;
    }

    fprintf(f, "%s %ld %s\n", kv->access_token, kv->expire_at, kv->refresh_token);
    fclose(f);
    return 0;
}

static void on_item_changed(void* ctx, const char* item_json)
{
    (void)ctx;
    printf("%s\n", item_json);
}

static int is_folder_path(const char* path)
{
    size_t len = strlen(path);
    return len > 0 && path[len - 1] == '/';
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "usage: %s <item_path> [select]\n", argv[0]);
        fprintf(stderr, "  file:   %s /Documents/file.txt\n", argv[0]);
        fprintf(stderr, "  folder: %s /Documents/ [id,name,size]\n", argv[0]);
        return 1;
    }

    const char* item_path = argv[1];

    OneDriveClient* client = onedrive_client_create(TENANT, CLIENT_ID, SCOPES);
    if (!client)
    {
        fprintf(stderr, "failed to create client\n");
        return 1;
    }

    onedrive_auth_init(client, NULL, on_keyvault_load, on_keyvault_save);

    if (is_folder_path(item_path))
    {
        char* item_id = NULL;
        if (onedrive_get_item_id(client, item_path, &item_id) != 0)
        {
            fprintf(stderr, "failed to resolve folder id\n");
            onedrive_client_destroy(client);
            return 1;
        }

        const char* select = argc >= 3 ? argv[2] : NULL;

        char* delta_token = NULL;
        int rc = onedrive_get_delta(
            client,
            item_id,
            select,
            delta_token,
            /* ctx */ NULL,
            on_item_changed,
            &delta_token);

        free(item_id);

        if (rc != 0)
        {
            fprintf(stderr, "failed to get delta\n");
            onedrive_client_destroy(client);
            return 1;
        }

        printf("delta_token: %s\n", delta_token);
        free(delta_token);
    }
    else
    {
        char* url = NULL;
        if (onedrive_get_content_url(client, item_path, &url) != 0)
        {
            fprintf(stderr, "failed to resolve content url\n");
            onedrive_client_destroy(client);
            return 1;
        }

        printf("%s\n", url);
        free(url);
    }

    onedrive_client_destroy(client);
    return 0;
}
