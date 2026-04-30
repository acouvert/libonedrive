/*
 * test_onedrive_music.c — unit tests for onedrive_music.c
 *
 * Links against mock_http_client.c (no curl dependency) and provides
 * a mock audio_stream_read_metadata (no libaudiotag dependency).
 * Each test function returns 0 on success, -1 on failure.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "onedrive_music.h"
#include "http_client.h"
#include "mock_http_client.h"
#include "audio_metadata.h"
#include "audio_stream.h"

/* ------------------------------------------------------------------ */
/*  Test macros                                                        */
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
/*  Mock: audio_stream_read_metadata                                   */
/* ------------------------------------------------------------------ */

typedef struct {
    const char* key;
    const char* value;
} MockTag;

#define MOCK_MAX_TAGS 16

static MockTag g_mock_tags[MOCK_MAX_TAGS];
static int     g_mock_tag_count;
static int     g_mock_metadata_rc;

static void mock_audio_reset(void)
{
    g_mock_tag_count  = 0;
    g_mock_metadata_rc = 0;
}

static void mock_audio_add_tag(const char* key, const char* value)
{
    if (g_mock_tag_count < MOCK_MAX_TAGS)
    {
        g_mock_tags[g_mock_tag_count].key   = key;
        g_mock_tags[g_mock_tag_count].value = value;
        g_mock_tag_count++;
    }
}

/* Satisfies the audio_stream_read_metadata symbol from audio_metadata.h */
int audio_stream_read_metadata(
    AudioStream* stream,
    void* ctx,
    tag_found_cb cb)
{
    (void)stream;

    if (g_mock_metadata_rc != 0)
    {
        return g_mock_metadata_rc;
    }

    for (int i = 0; i < g_mock_tag_count; i++)
    {
        cb(ctx, g_mock_tags[i].key, g_mock_tags[i].value);
    }

    return 0;
}

/* ------------------------------------------------------------------ */
/*  Auth stub                                                          */
/* ------------------------------------------------------------------ */

static const char* SCOPES = "Files.Read offline_access";

static int load_valid_token(void* ctx, OneDriveKeyVault* kv)
{
    (void)ctx;
    kv->access_token  = strdup("valid_access_token");
    kv->expire_at     = 2000000000L;
    kv->refresh_token = strdup("valid_refresh_token");
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Tests: onedrive_get_music_delta — parameter validation             */
/* ------------------------------------------------------------------ */

static int test_music_delta_null_client(void)
{
    char* dt = NULL;
    ASSERT(onedrive_get_music_delta(NULL, "id", NULL, NULL, NULL, &dt) == -1);
    return 0;
}

static int test_music_delta_null_item_id(void)
{
    OneDriveClient* c = onedrive_client_create("consumers", "id", SCOPES);
    ASSERT(c != NULL);
    char* dt = NULL;
    ASSERT(onedrive_get_music_delta(c, NULL, NULL, NULL, NULL, &dt) == -1);
    onedrive_client_destroy(c);
    return 0;
}

static int test_music_delta_null_out_token(void)
{
    OneDriveClient* c = onedrive_client_create("consumers", "id", SCOPES);
    ASSERT(c != NULL);
    ASSERT(onedrive_get_music_delta(c, "id", NULL, NULL, NULL, NULL) == -1);
    onedrive_client_destroy(c);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Tests: onedrive_get_music_delta — filtering                        */
/* ------------------------------------------------------------------ */

static int    g_music_item_count;
static char*  g_last_item_id;
static char*  g_last_item_version;
static char*  g_last_item_url;
static OneDriveDeltaAction g_last_action;

static void music_item_recorder(
    void* ctx,
    OneDriveDeltaAction action,
    const char* item_id,
    const char* item_version,
    const char* item_url)
{
    (void)ctx;
    g_music_item_count++;
    g_last_action = action;

    free(g_last_item_id);
    free(g_last_item_version);
    free(g_last_item_url);

    g_last_item_id      = item_id      ? strdup(item_id)      : NULL;
    g_last_item_version = item_version ? strdup(item_version) : NULL;
    g_last_item_url     = item_url     ? strdup(item_url)     : NULL;
}

static void reset_music_item_recorder(void)
{
    g_music_item_count = 0;
    free(g_last_item_id);       g_last_item_id      = NULL;
    free(g_last_item_version);  g_last_item_version = NULL;
    free(g_last_item_url);      g_last_item_url     = NULL;
}

static int test_music_delta_filters_non_music(void)
{
    OneDriveClient* c = onedrive_client_create("consumers", "id", SCOPES);
    ASSERT(c != NULL);
    onedrive_auth_init(c, NULL, load_valid_token, NULL);

    mock_http_reset();
    /* Mix of music and non-music items; only .mp3 and .flac should pass */
    mock_http_queue_response(200,
        "{\"value\":["
        "{\"id\":\"1\",\"name\":\"song.mp3\",\"cTag\":\"v1\",\"@microsoft.graph.downloadUrl\":\"https://dl/1\"},"
        "{\"id\":\"2\",\"name\":\"readme.txt\",\"cTag\":\"v2\"},"
        "{\"id\":\"3\",\"name\":\"photo.jpg\",\"cTag\":\"v3\"},"
        "{\"id\":\"4\",\"name\":\"track.flac\",\"cTag\":\"v4\",\"@microsoft.graph.downloadUrl\":\"https://dl/4\"},"
        "{\"id\":\"5\",\"name\":\"doc.pdf\",\"cTag\":\"v5\"}"
        "],"
        "\"@odata.deltaLink\":\"https://graph.microsoft.com/delta?token=tok1\"}");

    reset_music_item_recorder();
    char* dt = NULL;
    ASSERT(onedrive_get_music_delta(c, "root_id", NULL, NULL, music_item_recorder, &dt) == 0);
    ASSERT(dt != NULL);
    ASSERT(strcmp(dt, "tok1") == 0);
    ASSERT(g_music_item_count == 2);
    free(dt);

    reset_music_item_recorder();
    onedrive_client_destroy(c);
    return 0;
}

static int test_music_delta_skips_folders(void)
{
    OneDriveClient* c = onedrive_client_create("consumers", "id", SCOPES);
    ASSERT(c != NULL);
    onedrive_auth_init(c, NULL, load_valid_token, NULL);

    mock_http_reset();
    /* Folder items have no cTag — should be skipped even if name looks like music */
    mock_http_queue_response(200,
        "{\"value\":["
        "{\"id\":\"f1\",\"name\":\"Music.mp3\"},"
        "{\"id\":\"f2\",\"name\":\"real.mp3\",\"cTag\":\"c1\",\"@microsoft.graph.downloadUrl\":\"https://dl/f2\"}"
        "],"
        "\"@odata.deltaLink\":\"https://graph.microsoft.com/delta?token=tok2\"}");

    reset_music_item_recorder();
    char* dt = NULL;
    ASSERT(onedrive_get_music_delta(c, "root_id", NULL, NULL, music_item_recorder, &dt) == 0);
    ASSERT(g_music_item_count == 1);
    ASSERT(strcmp(g_last_item_id, "f2") == 0);
    free(dt);

    reset_music_item_recorder();
    onedrive_client_destroy(c);
    return 0;
}

static int test_music_delta_extracts_fields(void)
{
    OneDriveClient* c = onedrive_client_create("consumers", "id", SCOPES);
    ASSERT(c != NULL);
    onedrive_auth_init(c, NULL, load_valid_token, NULL);

    mock_http_reset();
    mock_http_queue_response(200,
        "{\"value\":["
        "{\"id\":\"item42\",\"name\":\"test.MP3\",\"cTag\":\"ver99\","
        "\"@microsoft.graph.downloadUrl\":\"https://cdn.example.com/test.mp3\"}"
        "],"
        "\"@odata.deltaLink\":\"https://graph.microsoft.com/delta?token=tok3\"}");

    reset_music_item_recorder();
    char* dt = NULL;
    ASSERT(onedrive_get_music_delta(c, "root_id", NULL, NULL, music_item_recorder, &dt) == 0);
    ASSERT(g_music_item_count == 1);
    ASSERT(strcmp(g_last_item_id, "item42") == 0);
    ASSERT(strcmp(g_last_item_version, "ver99") == 0);
    ASSERT(strcmp(g_last_item_url, "https://cdn.example.com/test.mp3") == 0);
    free(dt);

    reset_music_item_recorder();
    onedrive_client_destroy(c);
    return 0;
}

static int test_music_delta_case_insensitive_ext(void)
{
    OneDriveClient* c = onedrive_client_create("consumers", "id", SCOPES);
    ASSERT(c != NULL);
    onedrive_auth_init(c, NULL, load_valid_token, NULL);

    mock_http_reset();
    mock_http_queue_response(200,
        "{\"value\":["
        "{\"id\":\"1\",\"name\":\"SONG.FLAC\",\"cTag\":\"c1\",\"@microsoft.graph.downloadUrl\":\"https://dl/1\"},"
        "{\"id\":\"2\",\"name\":\"Mix.Mp3\",\"cTag\":\"c2\",\"@microsoft.graph.downloadUrl\":\"https://dl/2\"}"
        "],"
        "\"@odata.deltaLink\":\"https://graph.microsoft.com/delta?token=tok4\"}");

    reset_music_item_recorder();
    char* dt = NULL;
    ASSERT(onedrive_get_music_delta(c, "root_id", NULL, NULL, music_item_recorder, &dt) == 0);
    ASSERT(g_music_item_count == 2);
    free(dt);

    reset_music_item_recorder();
    onedrive_client_destroy(c);
    return 0;
}

static int test_music_delta_null_callback(void)
{
    OneDriveClient* c = onedrive_client_create("consumers", "id", SCOPES);
    ASSERT(c != NULL);

    char* dt = NULL;
    /* NULL callback — must return -1 */
    ASSERT(onedrive_get_music_delta(c, "root_id", NULL, NULL, NULL, &dt) == -1);

    onedrive_client_destroy(c);
    return 0;
}

static int test_music_delta_no_name(void)
{
    OneDriveClient* c = onedrive_client_create("consumers", "id", SCOPES);
    ASSERT(c != NULL);
    onedrive_auth_init(c, NULL, load_valid_token, NULL);

    mock_http_reset();
    /* Item without a name field — should be skipped */
    mock_http_queue_response(200,
        "{\"value\":["
        "{\"id\":\"1\",\"cTag\":\"c1\"}"
        "],"
        "\"@odata.deltaLink\":\"https://graph.microsoft.com/delta?token=tok6\"}");

    reset_music_item_recorder();
    char* dt = NULL;
    ASSERT(onedrive_get_music_delta(c, "root_id", NULL, NULL, music_item_recorder, &dt) == 0);
    ASSERT(g_music_item_count == 0);
    free(dt);

    reset_music_item_recorder();
    onedrive_client_destroy(c);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Metadata callback recorder                                         */
/* ------------------------------------------------------------------ */

typedef struct {
    char* item_id;
    char* name;
    char* value;
} RecordedMetadata;

#define MAX_RECORDED_METADATA 16

static RecordedMetadata g_recorded_metadata[MAX_RECORDED_METADATA];
static int g_recorded_metadata_count;

static void music_metadata_recorder(
    void* ctx,
    const char* item_id,
    const char* metadata_name,
    const char* metadata_value)
{
    (void)ctx;
    if (g_recorded_metadata_count < MAX_RECORDED_METADATA)
    {
        g_recorded_metadata[g_recorded_metadata_count].item_id = item_id ? strdup(item_id) : NULL;
        g_recorded_metadata[g_recorded_metadata_count].name    = metadata_name ? strdup(metadata_name) : NULL;
        g_recorded_metadata[g_recorded_metadata_count].value   = metadata_value ? strdup(metadata_value) : NULL;
        g_recorded_metadata_count++;
    }
}

static void reset_metadata_recorder(void)
{
    for (int i = 0; i < g_recorded_metadata_count; i++)
    {
        free(g_recorded_metadata[i].item_id);
        free(g_recorded_metadata[i].name);
        free(g_recorded_metadata[i].value);
    }

    g_recorded_metadata_count = 0;
}

/* ------------------------------------------------------------------ */
/*  Tests: onedrive_music_get_metadata — parameter validation          */
/* ------------------------------------------------------------------ */

static int test_metadata_null_client(void)
{
    ASSERT(onedrive_music_get_metadata(NULL, "id", "https://dl", NULL, NULL, music_metadata_recorder) == -1);
    return 0;
}

static int test_metadata_null_item_id(void)
{
    OneDriveClient* c = onedrive_client_create("consumers", "id", SCOPES);
    ASSERT(c != NULL);
    ASSERT(onedrive_music_get_metadata(c, NULL, "https://dl", NULL, NULL, music_metadata_recorder) == -1);
    onedrive_client_destroy(c);
    return 0;
}

static int test_metadata_null_item_url(void)
{
    OneDriveClient* c = onedrive_client_create("consumers", "id", SCOPES);
    ASSERT(c != NULL);
    ASSERT(onedrive_music_get_metadata(c, "id", NULL, NULL, NULL, music_metadata_recorder) == -1);
    onedrive_client_destroy(c);
    return 0;
}

static int test_metadata_null_callback(void)
{
    OneDriveClient* c = onedrive_client_create("consumers", "id", SCOPES);
    ASSERT(c != NULL);
    ASSERT(onedrive_music_get_metadata(c, "id", "https://dl", NULL, NULL, NULL) == -1);
    onedrive_client_destroy(c);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Tests: onedrive_music_get_metadata — metadata extraction           */
/* ------------------------------------------------------------------ */

static int test_metadata_all_fields(void)
{
    OneDriveClient* c = onedrive_client_create("consumers", "id", SCOPES);
    ASSERT(c != NULL);

    mock_http_reset();
    mock_audio_reset();
    mock_audio_add_tag("title", "Test Song");
    mock_audio_add_tag("artist", "Test Artist");
    mock_audio_add_tag("album", "Test Album");

    reset_metadata_recorder();
    /* NULL metadata_fields → all tags forwarded */
    ASSERT(onedrive_music_get_metadata(c, "item1", "https://dl/song.mp3",
        NULL, NULL, music_metadata_recorder) == 0);
    ASSERT(g_recorded_metadata_count == 3);
    ASSERT(strcmp(g_recorded_metadata[0].item_id, "item1") == 0);
    ASSERT(strcmp(g_recorded_metadata[0].name, "title") == 0);
    ASSERT(strcmp(g_recorded_metadata[0].value, "Test Song") == 0);
    ASSERT(strcmp(g_recorded_metadata[1].name, "artist") == 0);
    ASSERT(strcmp(g_recorded_metadata[2].name, "album") == 0);

    reset_metadata_recorder();
    onedrive_client_destroy(c);
    return 0;
}

static int test_metadata_filter_fields(void)
{
    OneDriveClient* c = onedrive_client_create("consumers", "id", SCOPES);
    ASSERT(c != NULL);

    mock_http_reset();
    mock_audio_reset();
    mock_audio_add_tag("title", "Filtered Song");
    mock_audio_add_tag("artist", "Filtered Artist");
    mock_audio_add_tag("album", "Filtered Album");
    mock_audio_add_tag("year", "2024");

    reset_metadata_recorder();
    /* Only title and year requested */
    ASSERT(onedrive_music_get_metadata(c, "item2", "https://dl/song.mp3",
        "title,year", NULL, music_metadata_recorder) == 0);
    ASSERT(g_recorded_metadata_count == 2);
    ASSERT(strcmp(g_recorded_metadata[0].name, "title") == 0);
    ASSERT(strcmp(g_recorded_metadata[0].value, "Filtered Song") == 0);
    ASSERT(strcmp(g_recorded_metadata[1].name, "year") == 0);
    ASSERT(strcmp(g_recorded_metadata[1].value, "2024") == 0);

    reset_metadata_recorder();
    onedrive_client_destroy(c);
    return 0;
}

static int test_metadata_single_field(void)
{
    OneDriveClient* c = onedrive_client_create("consumers", "id", SCOPES);
    ASSERT(c != NULL);

    mock_http_reset();
    mock_audio_reset();
    mock_audio_add_tag("title", "Only Title");
    mock_audio_add_tag("artist", "Ignored");

    reset_metadata_recorder();
    ASSERT(onedrive_music_get_metadata(c, "item3", "https://dl/song.mp3",
        "title", NULL, music_metadata_recorder) == 0);
    ASSERT(g_recorded_metadata_count == 1);
    ASSERT(strcmp(g_recorded_metadata[0].name, "title") == 0);
    ASSERT(strcmp(g_recorded_metadata[0].value, "Only Title") == 0);

    reset_metadata_recorder();
    onedrive_client_destroy(c);
    return 0;
}

static int test_metadata_no_matching_fields(void)
{
    OneDriveClient* c = onedrive_client_create("consumers", "id", SCOPES);
    ASSERT(c != NULL);

    mock_http_reset();
    mock_audio_reset();
    mock_audio_add_tag("title", "Song");
    mock_audio_add_tag("artist", "Artist");

    reset_metadata_recorder();
    /* Request a field that doesn't exist in the tags */
    ASSERT(onedrive_music_get_metadata(c, "item4", "https://dl/song.mp3",
        "genre,year", NULL, music_metadata_recorder) == 0);
    ASSERT(g_recorded_metadata_count == 0);

    reset_metadata_recorder();
    onedrive_client_destroy(c);
    return 0;
}

static int test_metadata_audio_error(void)
{
    OneDriveClient* c = onedrive_client_create("consumers", "id", SCOPES);
    ASSERT(c != NULL);

    mock_http_reset();
    mock_audio_reset();
    g_mock_metadata_rc = -1; /* simulate audio parsing failure */

    reset_metadata_recorder();
    ASSERT(onedrive_music_get_metadata(c, "item5", "https://dl/bad.mp3",
        NULL, NULL, music_metadata_recorder) == -1);
    ASSERT(g_recorded_metadata_count == 0);

    reset_metadata_recorder();
    onedrive_client_destroy(c);
    return 0;
}

/* ------------------------------------------------------------------ */
/*  main                                                               */
/* ------------------------------------------------------------------ */

int main(void)
{
    int passed = 0, failed = 0;

    /* onedrive_get_music_delta — parameter validation */
    RUN_TEST(test_music_delta_null_client);
    RUN_TEST(test_music_delta_null_item_id);
    RUN_TEST(test_music_delta_null_out_token);

    /* onedrive_get_music_delta — filtering */
    RUN_TEST(test_music_delta_filters_non_music);
    RUN_TEST(test_music_delta_skips_folders);
    RUN_TEST(test_music_delta_extracts_fields);
    RUN_TEST(test_music_delta_case_insensitive_ext);
    RUN_TEST(test_music_delta_null_callback);
    RUN_TEST(test_music_delta_no_name);

    /* onedrive_music_get_metadata — parameter validation */
    RUN_TEST(test_metadata_null_client);
    RUN_TEST(test_metadata_null_item_id);
    RUN_TEST(test_metadata_null_item_url);
    RUN_TEST(test_metadata_null_callback);

    /* onedrive_music_get_metadata — metadata extraction */
    RUN_TEST(test_metadata_all_fields);
    RUN_TEST(test_metadata_filter_fields);
    RUN_TEST(test_metadata_single_field);
    RUN_TEST(test_metadata_no_matching_fields);
    RUN_TEST(test_metadata_audio_error);

    fprintf(stderr, "  %-50s%s\n", "---", "--");
    fprintf(stderr, "  %d passed, %d failed\n\n", passed, failed);

    return failed;
}
