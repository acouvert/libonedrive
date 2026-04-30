#include "onedrive_music.h"
#include "json_utils.h"
#include "http_client.h"

#include "audio_metadata.h"
#include "audio_stream.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define CHUNK_SIZE (16 * 1024)

/* ------------------------------------------------------------------ */
/*  Memory audio stream (streaming HTTP range reader)                  */
/* ------------------------------------------------------------------ */

struct s_memory_audio_stream {
    AudioStream  base;
    HttpClient*  http;
    char*        url;
    Buffer       buf;
};

static int memory_stream_read(
    const AudioStream* stream,
    size_t offset,
    size_t length,
    uint8_t* out_buffer)
{
    struct s_memory_audio_stream* ms = (struct s_memory_audio_stream*)stream;

    size_t end    = offset + length;
    size_t copied = 0;

    /* Copy bytes already in the buffer */
    if (offset < ms->buf.size)
    {
        size_t avail = ms->buf.size - offset;
        if (avail > length)
        {
            avail = length;
        }

        memcpy(out_buffer, ms->buf.data + offset, avail);
        copied += avail;
    }

    /* Fetch more if needed */
    while (copied < length)
    {
        size_t fetch_end = end;
        if (fetch_end < ms->buf.size + CHUNK_SIZE)
        {
            fetch_end = ms->buf.size + CHUNK_SIZE;
        }

        size_t prev_size = ms->buf.size;

        long status = http_get_range(
            ms->http,
            ms->url,
            ms->buf.size,
            fetch_end - 1,
            &ms->buf);

        if (ms->buf.size == prev_size || (status != 200 && status != 206))
        {
            return -1;
        }

        /* Copy newly downloaded bytes */
        size_t src = offset + copied;
        if (src >= ms->buf.size)
        {
            return -1;
        }

        size_t avail = ms->buf.size - src;
        size_t need  = length - copied;
        if (avail > need)
        {
            avail = need;
        }

        memcpy(out_buffer + copied, ms->buf.data + src, avail);
        copied += avail;
    }

    return 0;
}

static struct s_memory_audio_stream* memory_audio_stream_create(const char* url)
{
    struct s_memory_audio_stream* ms = calloc(1, sizeof(*ms));
    if (!ms)
    {
        return NULL;
    }

    ms->http = http_client_create();
    if (!ms->http)
    {
        free(ms);
        return NULL;
    }

    ms->url = strdup(url);
    if (!ms->url)
    {
        http_client_destroy(ms->http);
        free(ms);
        return NULL;
    }

    ms->base.read = memory_stream_read;
    return ms;
}

static void memory_audio_stream_destroy(struct s_memory_audio_stream* ms)
{
    if (!ms)
    {
        return;
    }

    http_client_destroy(ms->http);
    buffer_free(&ms->buf);
    free(ms->url);
    free(ms);
}

/* ------------------------------------------------------------------ */
/*  Music file detection                                               */
/* ------------------------------------------------------------------ */

static int is_music_file(const char* name)
{
    if (!name)
    {
        return 0;
    }

    const char* dot = strrchr(name, '.');
    if (!dot)
    {
        return 0;
    }

    return strcasecmp(dot, ".mp3")  == 0
        || strcasecmp(dot, ".flac") == 0;
}

/* ------------------------------------------------------------------ */
/*  Internal delta callback                                            */
/* ------------------------------------------------------------------ */

struct s_music_delta_ctx {
    void* user_ctx;
    onedrive_music_item_cb user_cb;
};

static void music_delta_on_item(void* ctx, OneDriveDeltaAction action, const char* item_json)
{
    struct s_music_delta_ctx* music_delta_ctx = ctx;

    JsonDoc* doc = json_parse(item_json);
    if (!doc)
    {
        return;
    }

    char* item_id = NULL;
    if (json_doc_get_string(doc, "id", &item_id) != 0)
    {
        json_free(doc);
        return;
    }

    if (action == ONEDRIVE_DELTA_DELETE)
    {
        music_delta_ctx->user_cb(music_delta_ctx->user_ctx, action, item_id, NULL, NULL);

        free(item_id);
        json_free(doc);
        return;
    }

    /* Must have a name to determine file type. */
    char* name = NULL;
    if (json_doc_get_string(doc, "name", &name) != 0)
    {
        free(item_id);
        json_free(doc);
        return;
    }

    /* Skip folders and non-music files. */
    /* cTag property isn't returned if the item is a folder */
    if (!json_doc_has_key(doc, "cTag") || !is_music_file(name))
    {
        free(name);
        free(item_id);
        json_free(doc);
        return;
    }

    free(name);

    /* Extract version (cTag) and download URL. */
    char* item_version = NULL;
    char* item_url = NULL;

    json_doc_get_string(doc, "cTag", &item_version);
    json_doc_get_string(doc, "@microsoft.graph.downloadUrl", &item_url);

    music_delta_ctx->user_cb(music_delta_ctx->user_ctx, action, item_id, item_version, item_url);

    free(item_url);
    free(item_version);
    free(item_id);
    json_free(doc);
}

/* ------------------------------------------------------------------ */
/*  Public API                                                         */
/* ------------------------------------------------------------------ */

int onedrive_get_music_delta(
    OneDriveClient* client,
    const char* item_id,
    const char* delta_token,
    void* cb_ctx,
    onedrive_music_item_cb on_item_changed,
    char** out_delta_token)
{
    if (!client || !item_id || !on_item_changed || !out_delta_token)
    {
        return -1;
    }

    struct s_music_delta_ctx user_ctx = {
        .user_ctx = cb_ctx,
        .user_cb  = on_item_changed,
    };

    return onedrive_get_delta(
        client,
        item_id,
        "id,name,deleted,cTag,@microsoft.graph.downloadUrl",
        delta_token,
        &user_ctx,
        music_delta_on_item,
        out_delta_token);
}

/* ------------------------------------------------------------------ */
/*  Metadata extraction                                                */
/* ------------------------------------------------------------------ */

struct s_metadata_ctx {
    const char*  item_id;
    char** fields;      /* array of field names, or NULL for all */
    size_t field_count;
    void* user_ctx;
    onedrive_music_item_metadata_cb user_cb;
};

static int field_matches(const struct s_metadata_ctx* ctx, const char* name)
{
    for (size_t i = 0; i < ctx->field_count; i++)
    {
        if (strcmp(ctx->fields[i], name) == 0)
        {
            return 1;
        }
    }

    return 0;
}

static void metadata_on_tag(void* ctx, const char* key, const char* value)
{
    struct s_metadata_ctx* metadata_ctx = ctx;

    if (metadata_ctx->fields && !field_matches(metadata_ctx, key))
    {
        return;
    }

    metadata_ctx->user_cb(metadata_ctx->user_ctx, metadata_ctx->item_id, key, value);
}

static int split_fields(const char* metadata_fields, char*** out_fields, size_t* out_count)
{
    *out_fields = NULL;
    *out_count = 0;

    if (!metadata_fields)
    {
        return 0;
    }

   /* Count fields */
    size_t count = 1;
    for (const char* p = metadata_fields; *p; p++)
    {
        if (*p == ',')
        {
            count++;
        }
    }

    *out_fields = malloc(count * sizeof(char*));
    if (!*out_fields)
    {
        return -1;
    }

    (*out_fields)[0] = strdup(metadata_fields);
    if (!(*out_fields)[0])
    {
        free(*out_fields);
        return -1;
    }

    /* Split in place: replace ',' with '\0' and record pointers */
    *out_count = 1;
    for (char* p = (*out_fields)[0]; *p; p++)
    {
        if (*p == ',')
        {
            *p = '\0';
            (*out_fields)[*out_count] = p + 1;
            (*out_count)++;
        }
    }

    return 0;
}

int onedrive_music_get_metadata(
    OneDriveClient* client,
    const char* item_id,
    const char* item_url,
    const char* metadata_fields,
    void* cb_ctx,
    onedrive_music_item_metadata_cb on_metadata)
{
    if (!client || !item_id || !item_url || !on_metadata)
    {
        return -1;
    }

    struct s_metadata_ctx metadata_ctx = {
        .item_id     = item_id,
        .fields      = NULL,
        .field_count = 0,
        .user_ctx    = cb_ctx,
        .user_cb     = on_metadata,
    };

    int rc = split_fields(metadata_fields, &metadata_ctx.fields, &metadata_ctx.field_count);
    if (rc == 0)
    {       
        struct s_memory_audio_stream* stream = memory_audio_stream_create(item_url);
        if (stream)
        {
            rc = audio_stream_read_metadata(
                (AudioStream*)stream,
                &metadata_ctx,
                metadata_on_tag);

            memory_audio_stream_destroy(stream);
        }
        else
        {
            rc = -1;
        }
    }

    if (metadata_ctx.fields)
    {
        /* fields[0] is the backing buffer for all entries */
        free(metadata_ctx.fields[0]);
        free(metadata_ctx.fields);
    }

    return rc;
}
