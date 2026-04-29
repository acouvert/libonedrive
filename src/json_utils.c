#include "json_utils.h"

#include <cjson/cJSON.h>
#include <stdlib.h>
#include <string.h>

struct JsonDoc {
    cJSON* root;
};

JsonDoc* json_parse(const char* json)
{
    if (!json)
    {
        return NULL;
    }

    cJSON* root = cJSON_Parse(json);
    if (!root)
    {
        return NULL;
    }

    JsonDoc* doc = malloc(sizeof(JsonDoc));
    if (!doc)
    {
        cJSON_Delete(root);
        return NULL;
    }

    doc->root = root;
    return doc;
}

void json_free(JsonDoc* doc)
{
    if (!doc)
    {
        return;
    }

    cJSON_Delete(doc->root);
    free(doc);
}

int json_doc_get_string(const JsonDoc* doc, const char* key, char** out)
{
    if (!doc || !key || !out)
    {
        return -1;
    }

    cJSON* item = cJSON_GetObjectItemCaseSensitive(doc->root, key);
    if (cJSON_IsString(item) && item->valuestring)
    {
        *out = strdup(item->valuestring);
        if (*out)
        {
            return 0;
        }
    }

    return -1;
}

int json_doc_get_int(const JsonDoc* doc, const char* key, int* out)
{
    if (!doc || !key || !out)
    {
        return -1;
    }

    cJSON* item = cJSON_GetObjectItemCaseSensitive(doc->root, key);
    if (cJSON_IsNumber(item))
    {
        *out = item->valueint;
        return 0;
    }

    return -1;
}

int json_doc_foreach_array_item(
    const JsonDoc* doc,
    const char* key,
    void* ctx,
    json_array_item_cb cb)
{
    if (!doc || !key || !cb)
    {
        return -1;
    }

    cJSON* arr = cJSON_GetObjectItemCaseSensitive(doc->root, key);
    int count = 0;
    cJSON* elem = NULL;
    cJSON_ArrayForEach(elem, arr)
    {
        char* elem_str = cJSON_PrintUnformatted(elem);
        if (elem_str)
        {
            cb(ctx, elem_str);
            cJSON_free(elem_str);
            count++;
        }
    }

    return count > 0 ? 0 : -1;
}
