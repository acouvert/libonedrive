# OneDrive Client Library

A C library for accessing Microsoft OneDrive files via the Microsoft Graph API. Authenticates using the OAuth 2.0 device authorization flow and supports token persistence through caller-provided callbacks.

## Building

Requires a C99 compiler, POSIX environment (Linux, macOS, WSL), libcurl, [cJSON](https://github.com/DaveGamble/cJSON), and `gcov` for coverage.

On Debian/Ubuntu:

```sh
sudo apt-get install libcurl4-openssl-dev libcjson-dev
```

```sh
make            # builds static lib, shared lib, and sample programs
make test       # builds and runs unit tests
make coverage   # builds with gcov, runs tests, prints per-file coverage summary
make clean      # removes all build artifacts
```

Output (in `build/`):
- `libonedrive.a` — static library
- `libonedrive.so` — shared library (built with `-fPIC -fvisibility=hidden`)
- `sample/main` — example program that resolves a file path to a download URL or lists changes in a folder

To link against the library:

```sh
cc -o myapp myapp.c -Ilibonedrive/src -Llibonedrive/build -lonedrive -lcurl -lcjson
```

## Public API

### OneDriveClient (onedrive.h)

```c
OneDriveClient* onedrive_client_create(
    const char* tenant,
    const char* client_id,
    const char* scopes);

void onedrive_client_destroy(OneDriveClient* client);
```

- `onedrive_client_create` — Allocates a new client configured for the given Azure AD tenant, application client ID, and OAuth scopes (space-separated string, e.g. `"Files.Read offline_access"`). Returns `NULL` on allocation failure or if any argument is `NULL`.
- `onedrive_client_destroy` — Frees all resources owned by the client. Safe to call with `NULL`.

### Token Persistence

```c
typedef struct {
    char* access_token;
    long  expire_at;
    char* refresh_token;
} OneDriveKeyVault;

typedef int (*onedrive_keyvault_load_cb)(void* ctx, OneDriveKeyVault* kv);
typedef int (*onedrive_keyvault_save_cb)(void* ctx, const OneDriveKeyVault* kv);

void onedrive_auth_init(
    OneDriveClient* client,
    void* onedrive_keyvault_ctx,
    onedrive_keyvault_load_cb on_keyvault_load,
    onedrive_keyvault_save_cb on_keyvault_save);
```

- `onedrive_auth_init` — Registers callbacks for loading and saving OAuth tokens. If `on_keyvault_load` is provided, it is called immediately to restore any previously persisted tokens. The `onedrive_keyvault_ctx` pointer is passed through to both callbacks.

### Content Resolution

```c
int onedrive_get_item_id(
    OneDriveClient* client,
    const char* item_path,
    char** out_item_id);
```

- `onedrive_get_item_id` — Resolves a OneDrive item path to its item ID. On success `*out_item_id` is set to a heap-allocated string and 0 is returned. Caller must free `*out_item_id` when done. Returns -1 on failure.

```c
int onedrive_get_content_url(
    OneDriveClient* client,
    const char* item_id,
    char** out_url);
```

- `onedrive_get_content_url` — Obtains a direct download URL for the given item ID. Handles token refresh automatically. On success `*out_url` is set to a heap-allocated string and 0 is returned. Caller must free `*out_url` when done. Returns -1 on failure.

### Delta Sync

```c
typedef void (*onedrive_delta_item_cb)(void* ctx, const char* item_json);

int onedrive_get_delta(
    OneDriveClient* client,
    const char* item_id,
    const char* select,
    const char* delta_token,
    void* cb_ctx,
    onedrive_delta_item_cb on_item_changed,
    char** out_delta_token);
```

- `onedrive_get_delta` — Tracks changes under a folder using the Microsoft Graph delta API. Pass `NULL` for `delta_token` on an initial sync; on subsequent calls pass the token returned by the previous invocation. `select` optionally limits the fields returned (comma-separated, may be `NULL`). For each changed item, `on_item_changed` is called with the item's JSON. On success `*out_delta_token` is set to a heap-allocated string and 0 is returned. Caller must free `*out_delta_token` when done. Returns -1 on failure. Handles pagination automatically.

## Authentication Flow

When no tokens are available, the library initiates a device authorization flow:

1. Requests a device code from the Microsoft identity platform.
2. Prints an instruction message for the user to visit a verification URL and enter a code.
3. Polls the token endpoint until the user completes authentication or the code expires.
4. Persists the obtained tokens via the `on_keyvault_save` callback.

On subsequent calls, if the access token has expired, the library silently refreshes it using the stored refresh token.