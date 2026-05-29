#ifndef ENGINE_VALUE_H
#define ENGINE_VALUE_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    V_NULL,
    V_BOOL,
    V_INT,
    V_STRING,
    V_OBJECT,  /* ordered key-value pairs */
    V_ARRAY
} ValueType;

typedef struct Value Value;

typedef struct {
    char  *key;
    Value *val;
} ValuePair;

struct Value {
    ValueType type;
    union {
        int         b;       /* V_BOOL */
        int64_t     i;       /* V_INT */
        char       *s;       /* V_STRING (owned) */
        struct {             /* V_OBJECT */
            ValuePair *pairs;
            size_t     count;
            size_t     cap;
        } obj;
        struct {             /* V_ARRAY */
            Value **items;
            size_t  count;
            size_t  cap;
        } arr;
    } u;
};

/* Constructors (caller owns the returned Value*) */
Value *val_null(void);
Value *val_bool(int b);
Value *val_int(int64_t i);
Value *val_string(const char *s);
Value *val_object(void);
Value *val_array(void);

/* Mutation */
int val_obj_set(Value *obj, const char *key, Value *val);
int val_arr_push(Value *arr, Value *item);

/* Access */
Value *val_obj_get(const Value *obj, const char *key);

/* Navigate a dot-separated path: "Cart.Items", "Store.Name" etc.
 * Returns NULL if not found. */
Value *val_get_path(const Value *root, const char *path);

/* Returns 1 if value is truthy (non-null, non-false, non-zero, non-empty) */
int val_is_truthy(const Value *v);

/* Write string representation into buf (caller must free).
 * Returns NULL on OOM. */
char *val_to_string(const Value *v);

void val_free(Value *v);

#endif /* ENGINE_VALUE_H */
