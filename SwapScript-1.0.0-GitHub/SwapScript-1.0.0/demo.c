#define SWAPSCRIPT_IMPLEMENTATION
#include "swapscript.h"
#include "swapscript_generated.h"

typedef struct { float conf_thresh, nms_iou; } ai_head_params_t;
static const ai_head_params_t HEAD_DAYLIGHT = {0.70f, 0.45f};
static const ai_head_params_t HEAD_NIGHT = {0.35f, 0.20f};

int main(void) {
    ss_engine_t engine;
    const uint32_t keys[] = {SS_METRIC_AMBIENT_LUX};
    const float lux_samples[] = {18.0f, 18.0f, 18.0f};
    ss_init(&engine);
    if (ss_register_context(&engine, SS_CONTEXT_DAYLIGHT, &HEAD_DAYLIGHT) != SS_OK ||
        ss_register_context(&engine, SS_CONTEXT_NIGHT, &HEAD_NIGHT) != SS_OK ||
        ss_load_rules(&engine, ss_generated_rules, SS_GENERATED_RULE_COUNT) != SS_OK ||
        ss_set_initial(&engine, SS_INITIAL_CONTEXT) != SS_OK) return 1;
    for (uint32_t t = 0; t < 3; ++t) {
        const float value[] = {lux_samples[t]};
        const ai_head_params_t *active = (const ai_head_params_t *)
            ss_process_event(&engine, keys, value, 1u, t * 25u);
        (void)active; /* Existing inference/controller consumes active parameters here. */
    }
    return 0;
}
