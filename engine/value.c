#include "engine/value.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

Value *val_null(void) {
    Value *v = calloc(1, sizeof(Value));
    if (v) v->type = V_NULL;
    return v;
}

Value *val_bool(int b) {
    Value *v = calloc(1, sizeof(Value));
    if (!v) return NULL;
    v->type = V_BOOL;
    v->u.b  = b ? 1 : 0;
    return v;
}

Value *val_int(int64_t i) {
    Value *v = calloc(1, sizeof(Value));
    if (!v) return NULL;
    v->type = V_INT;
    v->u.i  = i;
    return v;
}

Value *val_string(const char *s) {
    Value *v = calloc(1, sizeof(Value));
    if (!v) return NULL;
    v->type = V_STRING;
    v->u.s  = s ? strdup(s) : strdup("");
    if (!v->u.s) { free(v); return NULL; }
    return v;
}

Value *val_object(void) {
    Value *v = calloc(1, sizeof(Value));
    if (!v) return NULL;
    v->type = V_OBJECT;
    return v;
}

Value *val_array(void) {
    Value *v = calloc(1, sizeof(Value));
    if (!v) return NULL;
    v->type = V_ARRAY;
    return v;
}

int val_obj_set(Value *obj, const char *key, Value *val) {
    if (!obj || obj->type != V_OBJECT) return -1;
    /* Update existing */
    for (size_t i = 0; i < obj->u.obj.count; i++) {
        if (strcmp(obj->u.obj.pairs[i].key, key) == 0) {
            val_free(obj->u.obj.pairs[i].val);
            obj->u.obj.pairs[i].val = val;
            return 0;
        }
    }
    /* Add new */
    if (obj->u.obj.count >= obj->u.obj.cap) {
        size_t newcap = obj->u.obj.cap == 0 ? 8 : obj->u.obj.cap * 2;
        ValuePair *p = realloc(obj->u.obj.pairs, newcap * sizeof(ValuePair));
        if (!p) return -1;
        obj->u.obj.pairs = p;
        obj->u.obj.cap   = newcap;
    }
    char *k = strdup(key);
    if (!k) return -1;
    obj->u.obj.pairs[obj->u.obj.count].key = k;
    obj->u.obj.pairs[obj->u.obj.count].val = val;
    obj->u.obj.count++;
    return 0;
}

int val_arr_push(Value *arr, Value *item) {
    if (!arr || arr->type != V_ARRAY) return -1;
    if (arr->u.arr.count >= arr->u.arr.cap) {
        size_t newcap = arr->u.arr.cap == 0 ? 8 : arr->u.arr.cap * 2;
        Value **p = realloc(arr->u.arr.items, newcap * sizeof(Value *));
        if (!p) return -1;
        arr->u.arr.items = p;
        arr->u.arr.cap   = newcap;
    }
    arr->u.arr.items[arr->u.arr.count++] = item;
    return 0;
}

Value *val_obj_get(const Value *obj, const char *key) {
    if (!obj || obj->type != V_OBJECT) return NULL;
    for (size_t i = 0; i < obj->u.obj.count; i++) {
        if (strcmp(obj->u.obj.pairs[i].key, key) == 0)
            return obj->u.obj.pairs[i].val;
    }
    return NULL;
}

Value *val_get_path(const Value *root, const char *path) {
    if (!root || !path) return NULL;

    /* Make a mutable copy */
    char buf[512];
    snprintf(buf, sizeof(buf), "%s", path);

    const Value *cur = root;
    char *token = buf;
    char *dot;

    while (token && *token) {
        dot = strchr(token, '.');
        if (dot) *dot = '\0';

        cur = val_obj_get(cur, token);
        if (!cur) return NULL;

        token = dot ? dot + 1 : NULL;
    }
    return (Value *)cur;
}

int val_is_truthy(const Value *v) {
    if (!v) return 0;
    switch (v->type) {
    case V_NULL:   return 0;
    case V_BOOL:   return v->u.b;
    case V_INT:    return v->u.i != 0;
    case V_STRING: return v->u.s && v->u.s[0] != '\0';
    case V_OBJECT: return v->u.obj.count > 0;
    case V_ARRAY:  return v->u.arr.count > 0;
    }
    return 0;
}

char *val_to_string(const Value *v) {
    if (!v) return strdup("(null)");
    char tmp[64];
    switch (v->type) {
    case V_NULL:   return strdup("");
    case V_BOOL:   return strdup(v->u.b ? "true" : "false");
    case V_INT:
        snprintf(tmp, sizeof(tmp), "%lld", (long long)v->u.i);
        return strdup(tmp);
    case V_STRING: return strdup(v->u.s ? v->u.s : "");
    case V_OBJECT: return strdup("[object]");
    case V_ARRAY:  return strdup("[array]");
    }
    return strdup("");
}

void val_free(Value *v) {
    if (!v) return;
    switch (v->type) {
    case V_STRING:
        free(v->u.s);
        break;
    case V_OBJECT:
        for (size_t i = 0; i < v->u.obj.count; i++) {
            free(v->u.obj.pairs[i].key);
            val_free(v->u.obj.pairs[i].val);
        }
        free(v->u.obj.pairs);
        break;
    case V_ARRAY:
        for (size_t i = 0; i < v->u.arr.count; i++)
            val_free(v->u.arr.items[i]);
        free(v->u.arr.items);
        break;
    default:
        break;
    }
    free(v);
}
