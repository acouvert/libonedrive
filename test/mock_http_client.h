#ifndef _MOCK_HTTP_CLIENT_H_
#define _MOCK_HTTP_CLIENT_H_

/* Configure mock responses before calling code under test.
 * Each http_post/http_get call consumes the next queued response. */

#define MOCK_MAX_RESPONSES 8

void mock_http_reset(void);

/* Queue a response: status code + JSON body (copied internally). */
void mock_http_queue_response(long status, const char* body);

/* Set the redirect URL returned by http_client_get_redirect_url. */
void mock_http_set_redirect(const char* url);

/* Return how many http_post calls have been made since last reset. */
int mock_http_post_count(void);

/* Return how many http_get calls have been made since last reset. */
int mock_http_get_count(void);

#endif /* _MOCK_HTTP_CLIENT_H_ */
