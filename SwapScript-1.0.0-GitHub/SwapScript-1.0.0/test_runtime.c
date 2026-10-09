#define SWAPSCRIPT_IMPLEMENTATION
#include "swapscript.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>

typedef struct { int value; } params_t;
static const params_t A = {1}, B = {2};
enum { ID_A = 10u, ID_B = 20u, METRIC = 99u };
static const ss_rule_t rules[] = {
    {ID_A, ID_B, METRIC, 20.0f, 50u, SS_LE},
    {ID_B, ID_A, METRIC, 30.0f, 100u, SS_GE}
};

static ss_engine_t engine;
static void setup(void) {
    ss_init(&engine);
    assert(ss_register_context(&engine, ID_A, &A) == SS_OK);
    assert(ss_register_context(&engine, ID_B, &B) == SS_OK);
    assert(ss_load_rules(&engine, rules, 2u) == SS_OK);
    assert(ss_set_initial(&engine, ID_A) == SS_OK);
}
static const void *event(float value, uint32_t time) {
    const uint32_t keys[] = {METRIC};
    return ss_process_event(&engine, keys, &value, 1u, time);
}

int main(void) {
    setup();
    assert(ss_active_parameters(&engine) == &A);
    assert(event(19.0f, 100u) == &A); /* starts dwell */
    assert(event(19.0f, 149u) == &A); /* one ms early */
    assert(event(19.0f, 150u) == &B); /* dwell reached */
    assert(event(30.0f, 200u) == &B);
    assert(event(30.0f, 299u) == &B);
    assert(event(30.0f, 300u) == &A);

    /* Missing metric resets pending dwell rather than accumulating stale time. */
    assert(event(10.0f, 400u) == &A);
    assert(ss_process_event(&engine, NULL, NULL, 0u, 449u) == &A);
    assert(event(10.0f, 450u) == &A);
    assert(event(10.0f, 499u) == &A);
    assert(event(10.0f, 500u) == &B);

    /* Unsigned elapsed-time subtraction remains correct across tick wrap. */
    setup();
    assert(event(10.0f, 0xfffffff0u) == &A);
    assert(event(10.0f, 0x00000021u) == &A);
    assert(event(10.0f, 0x00000022u) == &B);
    return 0;
}
