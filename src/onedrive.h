#ifndef _ONEDRIVE_H_
#define _ONEDRIVE_H_

typedef struct OneDriveClient OneDriveClient;

/* Indicates whether a delta item was added/updated or deleted. */
typedef enum {
    ONEDRIVE_DELTA_UPSERT,
    ONEDRIVE_DELTA_DELETE
} OneDriveDeltaAction;

/* Token storage passed to keyvault callbacks. The library owns the heap
 * strings internally; callers that receive a const pointer in the changed
 * callback must copy any values they wish to persist. */
typedef struct {
    char* access_token;
    long  expire_at; /* Unix timestamp when access_token expires */
    char* refresh_token;
} OneDriveKeyVault;

/* Called to load previously persisted tokens.
 * Return 0 on success (kv populated), non-zero if no tokens are available. */
typedef int (*onedrive_keyvault_load_cb)(void* ctx, OneDriveKeyVault* kv);

/* Called whenever tokens are obtained or refreshed.
 * Return 0 on success, non-zero on failure. */
typedef int (*onedrive_keyvault_save_cb)(void* ctx, const OneDriveKeyVault* kv);

/* Called for each changed item during delta sync.
 * action indicates whether the item was created/updated or deleted.
 * item_json is the raw JSON for that item (valid for the duration of the call). */
typedef void (*onedrive_delta_item_cb)(void* ctx, OneDriveDeltaAction action, const char* item_json);

/* Create the OneDrive client. */
OneDriveClient* onedrive_client_create(
    const char* tenant,
    const char* client_id,
    const char* scopes);

/* Destroy the OneDrive client. */
void onedrive_client_destroy(OneDriveClient* client);

/* Register keyvault persistence callbacks.
 * Calls on_keyvault_load immediately if provided. */
void onedrive_auth_init(
    OneDriveClient* client,
    void* onedrive_keyvault_ctx,
    onedrive_keyvault_load_cb on_keyvault_load,
    onedrive_keyvault_save_cb on_keyvault_save);

/* Resolve item_path to its item ID.
 * On success the ID is written into out_item_id and 0 is returned.
 * Caller must free *out_item_id when done.
 * Returns -1 on failure. */
int onedrive_get_item_id(
    OneDriveClient* client,
    const char* item_path,
    char** out_item_id);

/* Obtain a direct download URL for the given item_id.
 * On success the URL is written into out_url and 0 is returned.
 * Caller must free *out_url when done.
 * Returns -1 on failure. */
int onedrive_get_content_url(
    OneDriveClient* client,
    const char* item_id,
    char** out_url);

/* Synchronise changes for item_id using the delta API.
 * delta_token should be zero-initialized for an initial sync; on subsequent
 * calls pass the token returned by the previous invocation.
 * select optionally limits the fields returned (comma-separated, may be NULL).
 * For each changed item, on_item_changed is invoked with cb_ctx and the
 * item's JSON.
 * On success the new delta token is written into delta_token and 0 is returned.
 * Returns -1 on failure. */
int onedrive_get_delta(
    OneDriveClient* client,
    const char* item_id,
    const char* select,
    const char* delta_token,
    void* cb_ctx,
    onedrive_delta_item_cb on_item_changed,
    char** out_delta_token);

#endif /* _ONEDRIVE_H_ */
