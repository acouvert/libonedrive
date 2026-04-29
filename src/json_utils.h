#ifndef _JSON_UTILS_H_
#define _JSON_UTILS_H_

/* Opaque handle to a parsed JSON document.
 * Parse once with json_parse, query with json_doc_get_*, free with json_free. */
typedef struct JsonDoc JsonDoc;

/* Called for each element in a JSON array found at the given key.
 * item_json is the serialised element (valid for the duration of the call). */
typedef void (*json_array_item_cb)(void* ctx, const char* item_json);

/* Parse a JSON string into a document handle.
 * Returns NULL on parse error or if json is NULL. */
JsonDoc* json_parse(const char* json);

/* Free a document handle returned by json_parse.
 * Safe to call with NULL. */
void json_free(JsonDoc* doc);

/* Extract a string value for key from a parsed document.
 * On success, *out is set to a heap-allocated copy (caller must free)
 * and 0 is returned.  Returns -1 if the key is missing, the value is
 * not a string, or on allocation error. */
int json_doc_get_string(const JsonDoc* doc, const char* key, char** out);

/* Extract an integer value for key from a parsed document.
 * On success, *out is set to the value and 0 is returned.
 * Returns -1 if the key is missing or not a number. */
int json_doc_get_int(const JsonDoc* doc, const char* key, int* out);

/* Look up the array at key in a parsed document and invoke cb for each element.
 * Returns 0 on success, or -1 if doc is NULL, key is
 * missing, the value is not an array, or cb is NULL. */
int json_doc_foreach_array_item(
    const JsonDoc* doc,
    const char* key,
    void* ctx,
    json_array_item_cb cb);

#endif /* _JSON_UTILS_H_ */
