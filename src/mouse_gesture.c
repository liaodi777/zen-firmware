#include <zephyr/device.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#include <drivers/input_processor.h>
#include <zmk/behavior.h>
#include <zmk/keymap.h>

#define DT_DRV_COMPAT zen_input_processor_mouse_gesture

#define GESTURE_RIGHT 0
#define GESTURE_LEFT  1
#define GESTURE_UP    2
#define GESTURE_DOWN  3

struct mouse_gesture_config {
    uint8_t layer;
    uint16_t threshold;
    struct zmk_behavior_binding bindings[4];
};

static int32_t accum_x;
static int32_t accum_y;

static void reset_accum(void) {
    accum_x = 0;
    accum_y = 0;
}

static void fire(const struct mouse_gesture_config *cfg, uint8_t direction) {
    const struct zmk_behavior_binding *binding = &cfg->bindings[direction];

    if (!binding->behavior_dev || binding->behavior_dev[0] == '\0') {
        return;
    }

    struct zmk_behavior_binding_event event = {
        .layer = cfg->layer,
        .position = 0,
        .timestamp = k_uptime_get(),
    };

    zmk_behavior_invoke_binding(binding, event, true);
    zmk_behavior_invoke_binding(binding, event, false);
}

static int handle_event(const struct device *dev, struct input_event *event,
                        uint32_t param1, uint32_t param2,
                        struct zmk_input_processor_state *state) {
    ARG_UNUSED(param1);
    ARG_UNUSED(param2);
    ARG_UNUSED(state);

    const struct mouse_gesture_config *cfg = dev->config;

    if (!zmk_keymap_layer_active(cfg->layer)) {
        reset_accum();
        return ZMK_INPUT_PROC_CONTINUE;
    }

    if (event->type != INPUT_EV_REL ||
        (event->code != INPUT_REL_X && event->code != INPUT_REL_Y)) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    if (event->code == INPUT_REL_X) {
        accum_x += event->value;
    } else {
        accum_y += event->value;
    }

    if ((ABS(accum_x) + ABS(accum_y)) < cfg->threshold) {
        return ZMK_INPUT_PROC_STOP;
    }

    uint8_t direction;
    if (ABS(accum_x) >= ABS(accum_y)) {
        direction = accum_x >= 0 ? GESTURE_RIGHT : GESTURE_LEFT;
    } else {
        direction = accum_y < 0 ? GESTURE_UP : GESTURE_DOWN;
    }

    reset_accum();
    fire(cfg, direction);

    return ZMK_INPUT_PROC_STOP;
}

static const struct zmk_input_processor_driver_api api = {
    .handle_event = handle_event,
};

#define BINDING_AT(node, idx)                                                                  \
    {                                                                                           \
        .behavior_dev = DEVICE_DT_NAME(DT_PHANDLE_BY_IDX(node, bindings, idx)),                \
        .param1 = COND_CODE_0(DT_PHA_HAS_CELL_AT_IDX(node, bindings, idx, param1), (0),        \
                              (DT_PHA_BY_IDX(node, bindings, idx, param1))),                    \
        .param2 = COND_CODE_0(DT_PHA_HAS_CELL_AT_IDX(node, bindings, idx, param2), (0),        \
                              (DT_PHA_BY_IDX(node, bindings, idx, param2))),                    \
    }

#define GESTURE_INIT(inst)                                                                      \
    static const struct mouse_gesture_config config_##inst = {                                  \
        .layer = DT_INST_PROP(inst, layer),                                                     \
        .threshold = DT_INST_PROP(inst, threshold),                                             \
        .bindings = {                                                                            \
            BINDING_AT(DT_DRV_INST(inst), 0),                                                    \
            BINDING_AT(DT_DRV_INST(inst), 1),                                                    \
            BINDING_AT(DT_DRV_INST(inst), 2),                                                    \
            BINDING_AT(DT_DRV_INST(inst), 3),                                                    \
        },                                                                                       \
    };                                                                                           \
    DEVICE_DT_INST_DEFINE(inst, NULL, NULL, NULL, &config_##inst,                              \
                          POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &api);

DT_INST_FOREACH_STATUS_OKAY(GESTURE_INIT)
