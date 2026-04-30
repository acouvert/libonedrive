#ifndef _HTTP_CLIENT_H_
#define _HTTP_CLIENT_H_

#include "buffer.h"

typedef struct HttpClient HttpClient;

/* Create an HTTP client backed by libcurl. */
HttpClient* http_client_create(void);

/* Destroy the HTTP client and release curl resources. */
void http_client_destroy(HttpClient* client);

/* Append a URL-encoded key=value pair to a form body buffer.
 * Pairs are separated by '&'.  The value is percent-encoded.
 * Returns 0 on success, -1 on failure. */
int http_form_add_param(HttpClient* client, const char* key, const char* value, Buffer* out);

/* Perform an HTTP POST with the given URL and form body.
 * The response body is written into out_response.
 * Returns the HTTP status code, or -1 on transport error. */
long http_post(HttpClient* client, const char* url, const char* body, Buffer* out_response);

/* Perform an HTTP GET with a bearer token for authorization.
 * If follow_redirects is non-zero, HTTP redirects are followed automatically.
 * The response body is written into out_response.
 * Returns the HTTP status code, or -1 on transport error. */
long http_get(HttpClient* client, const char* url, const char* bearer_token, int follow_redirects, Buffer* out_response);

/* Percent-encode str and write the result into out.
 * Returns 0 on success, -1 on failure. */
int http_client_url_encode(HttpClient* client, const char* str, Buffer* out);

/* Return a heap-allocated copy of the redirect URL from the last request.
 * *out_url must be NULL on entry; caller must free *out_url when done.
 * Returns 0 on success, -1 if no redirect URL is available. */
int http_client_get_redirect_url(HttpClient* client, char** out_url);

/* Extract the value of a query parameter from a URL.
 * Searches the query string of url for param and returns a heap-allocated
 * copy of its value in *out_value.  *out_value must be NULL on entry;
 * caller must free *out_value when done.
 * Returns 0 on success, -1 if the parameter is not found. */
int http_url_get_param(HttpClient* client, const char* url, const char* param, char** out_value);

/* Perform an HTTP GET requesting bytes [from, to] (inclusive) of the resource.
 * Follows redirects automatically.  The response body is appended into
 * out_response.
 * Returns the HTTP status code, or -1 on transport error. */
long http_get_range(
    HttpClient* client,
    const char* url,
    size_t from,
    size_t to,
    Buffer* out_response);

#endif /* _HTTP_CLIENT_H_ */
