#ifndef _ONEDRIVE_MUSIC_H_
#define _ONEDRIVE_MUSIC_H_

#include "onedrive.h"

/* Called for each music item discovered during delta sync.
 * action indicates whether the item was created/updated or deleted. */
typedef void (*onedrive_music_item_cb)(
    void* ctx,
    OneDriveDeltaAction action,
    const char* item_id,
    const char* item_version,
    const char* item_url);

/* Called for each extracted metadata field of a music item. */
typedef void (*onedrive_music_item_metadata_cb)(
    void* ctx,
    const char* item_id,
    const char* metadata_name,
    const char* metadata_value);

/* Like onedrive_get_delta, but only invokes on_item_changed for items whose
 * file extension indicates an audio file (.mp3, .flac).
 * Non-music items are silently skipped.
 * Returns 0 on success, -1 on failure. */
int onedrive_get_music_delta(
    OneDriveClient* client,
    const char* item_id,
    const char* delta_token,
    void* cb_ctx,
    onedrive_music_item_cb on_item_changed,
    char** out_delta_token);

/* Stream the music file at item_url and extract audio metadata without
 * downloading the entire file.
 * metadata_fields is a comma-separated list of metadata names to extract,
 * or NULL to receive all metadata found.
 * For each matching metadata, on_metadata is invoked with the item_id,
 * metadata name and metadata value.
 * Returns 0 on success, -1 on failure. */
int onedrive_music_get_metadata(
    OneDriveClient* client,
    const char* item_id,
    const char* item_url,
    const char* metadata_fields,
    void* cb_ctx,
    onedrive_music_item_metadata_cb on_metadata);

#endif /* _ONEDRIVE_MUSIC_H_ */
