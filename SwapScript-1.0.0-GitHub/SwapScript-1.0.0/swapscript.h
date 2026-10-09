/* SwapScript: allocation-free event rule engine. Define SWAPSCRIPT_IMPLEMENTATION
 * in exactly one C translation unit before including this file.
 */
#ifndef SWAPSCRIPT_H
#define SWAPSCRIPT_H

#include <stddef.h>
#include <stdint.h>
#include <math.h>

#ifndef SS_MAX_CONTEXTS
#define SS_MAX_CONTEXTS 16u
#endif
#ifndef SS_MAX_METRICS
#define SS_MAX_METRICS 32u
#endif

typedef enum { SS_LT, SS_LE, SS_GT, SS_GE, SS_EQ, SS_NE } ss_operator_t;
typedef struct {
    uint32_t from_id, to_id, metric_key;
    float threshold;
    uint32_t hold_ms;
    ss_operator_t op;
} ss_rule_t;
typedef struct { uint32_t id; const void *parameters; } ss_context_t;
typedef struct {
    ss_context_t contexts[SS_MAX_CONTEXTS];
    size_t context_count;
    const ss_rule_t *rules;
    size_t rule_count;
    uint32_t active_id;
    size_t pending_rule;
    uint32_t pending_since;
    uint8_t pending;
    const void *active_parameters;
} ss_engine_t;

/* Optional platform hooks. Defaults use GCC/Clang acquire/release atomics.
 * Define SS_POINTER_LOAD/SS_POINTER_STORE for a compiler/MCU-specific primitive.
 * A single thread/task must call ss_process_event; readers may load concurrently.
 */
#if !defined(SS_POINTER_LOAD) && !defined(SS_POINTER_STORE)
# if defined(__GNUC__) || defined(__clang__)
#  define SS_POINTER_LOAD(p) __atomic_load_n((p), __ATOMIC_ACQUIRE)
#  define SS_POINTER_STORE(p, v) __atomic_store_n((p), (v), __ATOMIC_RELEASE)
# else
#  define SS_POINTER_LOAD(p) (*(p))
#  define SS_POINTER_STORE(p, v) (*(p) = (v))
# endif
#elif !defined(SS_POINTER_LOAD) || !defined(SS_POINTER_STORE)
# error "Define both SS_POINTER_LOAD and SS_POINTER_STORE together"
#endif

typedef enum { SS_OK = 0, SS_ERR_ARGUMENT, SS_ERR_CAPACITY, SS_ERR_DUPLICATE,
               SS_ERR_CONTEXT, SS_ERR_STATE } ss_status_t;

void ss_init(ss_engine_t *engine);
ss_status_t ss_register_context(ss_engine_t *engine, uint32_t id, const void *parameters);
ss_status_t ss_load_rules(ss_engine_t *engine, const ss_rule_t *rules, size_t count);
ss_status_t ss_set_initial(ss_engine_t *engine, uint32_t id);
const void *ss_process_event(ss_engine_t *engine, const uint32_t *keys,
                             const float *values, size_t count, uint32_t now_ms);
const void *ss_active_parameters(const ss_engine_t *engine);
uint32_t ss_fnv1a(const char *text);

#ifdef SWAPSCRIPT_IMPLEMENTATION
#include <string.h>

uint32_t ss_fnv1a(const char *s) {
    uint32_t h = 2166136261u;
    if (!s) return 0u;
    while (*s) { h ^= (uint8_t)*s++; h *= 16777619u; }
    return h;
}
void ss_init(ss_engine_t *e) {
    if (e) { memset(e, 0, sizeof(*e)); e->rules = NULL; e->active_parameters = NULL; }
}
static int ss_has_context(const ss_engine_t *e, uint32_t id) {
    size_t i; for (i = 0; i < e->context_count; ++i) if (e->contexts[i].id == id) return 1;
    return 0;
}
ss_status_t ss_register_context(ss_engine_t *e, uint32_t id, const void *p) {
    size_t i;
    if (!e || !p) return SS_ERR_ARGUMENT;
    if (e->active_parameters) return SS_ERR_STATE;
    for (i = 0; i < e->context_count; ++i) if (e->contexts[i].id == id) return SS_ERR_DUPLICATE;
    if (e->context_count >= SS_MAX_CONTEXTS) return SS_ERR_CAPACITY;
    e->contexts[e->context_count].id = id;
    e->contexts[e->context_count++].parameters = p;
    return SS_OK;
}
ss_status_t ss_load_rules(ss_engine_t *e, const ss_rule_t *r, size_t n) {
    size_t i;
    if (!e || (n && !r)) return SS_ERR_ARGUMENT;
    if (e->active_parameters) return SS_ERR_STATE;
    for (i = 0; i < n; ++i)
        if (!ss_has_context(e, r[i].from_id) || !ss_has_context(e, r[i].to_id) ||
            r[i].from_id == r[i].to_id) return SS_ERR_CONTEXT;
    e->rules = r; e->rule_count = n; return SS_OK;
}
ss_status_t ss_set_initial(ss_engine_t *e, uint32_t id) {
    size_t i;
    if (!e) return SS_ERR_ARGUMENT;
    if (!ss_has_context(e, id)) return SS_ERR_CONTEXT;
    for (i = 0; i < e->context_count; ++i) if (e->contexts[i].id == id) {
        e->active_id = id;
        e->active_parameters = e->contexts[i].parameters;
        e->pending = 0u;
        return SS_OK;
    }
    return SS_ERR_CONTEXT;
}
static const float *ss_find_metric(const uint32_t *keys, const float *values,
                                   size_t n, uint32_t key) {
    size_t i; for (i = 0; i < n; ++i) if (keys[i] == key) return &values[i];
    return NULL;
}
static int ss_match(float x, ss_operator_t op, float t) {
    switch (op) { case SS_LT: return x < t; case SS_LE: return x <= t;
      case SS_GT: return x > t; case SS_GE: return x >= t;
      case SS_EQ: return x == t; case SS_NE: return x != t; default: return 0; }
}
const void *ss_process_event(ss_engine_t *e, const uint32_t *keys,
                             const float *values, size_t n, uint32_t now) {
    size_t i;
    if (!e || !e->active_parameters || n > SS_MAX_METRICS || (n && (!keys || !values))) return NULL;
    for (i = 0; i < e->rule_count; ++i) {
        const ss_rule_t *r = &e->rules[i];
        const float *v;
        if (r->from_id != e->active_id) continue;
        v = ss_find_metric(keys, values, n, r->metric_key);
        if (!v || !isfinite(*v) || !ss_match(*v, r->op, r->threshold)) continue;
        if (!e->pending || e->pending_rule != i) {
            e->pending = 1u; e->pending_rule = i; e->pending_since = now;
        }
        if ((uint32_t)(now - e->pending_since) >= r->hold_ms) {
            size_t j;
            for (j = 0; j < e->context_count; ++j) if (e->contexts[j].id == r->to_id) {
                e->active_id = r->to_id;
                SS_POINTER_STORE(&e->active_parameters, e->contexts[j].parameters);
                e->pending = 0u;
                return SS_POINTER_LOAD(&e->active_parameters);
            }
        }
        return SS_POINTER_LOAD(&e->active_parameters);
    }
    e->pending = 0u;
    return SS_POINTER_LOAD(&e->active_parameters);
}
const void *ss_active_parameters(const ss_engine_t *e) {
    return e ? SS_POINTER_LOAD(&e->active_parameters) : NULL;
}
#endif /* SWAPSCRIPT_IMPLEMENTATION */
#endif /* SWAPSCRIPT_H */
