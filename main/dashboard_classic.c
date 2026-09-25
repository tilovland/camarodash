#include "dashboard_classic.h"

#include "dashboard.h"

#define SHIFTLIGHT_COUNT 12

static lv_obj_t *s_rpm_label;
static lv_obj_t *s_rpm_bar;
static lv_obj_t *s_rpm_max_label;
static lv_obj_t *s_map_label;
static lv_obj_t *s_clt_label;
static lv_obj_t *s_lambda_label;
static lv_obj_t *s_map_range_label;
static lv_obj_t *s_clt_range_label;
static lv_obj_t *s_lambda_range_label;
static lv_obj_t *s_shiftlights[SHIFTLIGHT_COUNT];

typedef struct {
    bool rpm_valid;
    bool map_valid;
    bool clt_valid;
    bool lambda_valid;
    uint16_t rpm_min;
    uint16_t rpm_max;
    float map_min;
    float map_max;
    int16_t clt_min;
    int16_t clt_max;
    float lambda_min;
    float lambda_max;
} dashboard_stats_t;

static dashboard_stats_t s_stats;

static lv_obj_t *create_value_box(lv_obj_t *parent, int x, const char *title,
                                  lv_color_t color, lv_obj_t **range_label)
{
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_set_pos(box, x, 180);
    lv_obj_set_size(box, 220, 238);
    lv_obj_set_style_bg_color(box, lv_color_hex(0x080C10), 0);
    lv_obj_set_style_border_color(box, lv_color_hex(0x505860), 0);
    lv_obj_set_style_border_width(box, 2, 0);
    lv_obj_set_style_radius(box, 10, 0);

    lv_obj_t *heading = lv_label_create(box);
    lv_label_set_text(heading, title);
    lv_obj_set_style_text_color(heading, color, 0);
    lv_obj_set_style_text_font(heading, &lv_font_montserrat_20, 0);
    lv_obj_align(heading, LV_ALIGN_TOP_MID, 0, 8);

    lv_obj_t *value = lv_label_create(box);
    lv_obj_set_style_text_color(value, color, 0);
    lv_obj_set_style_text_font(value, &lv_font_montserrat_48, 0);
    lv_obj_align(value, LV_ALIGN_CENTER, 0, -5);

    *range_label = lv_label_create(box);
    lv_label_set_text(*range_label, "MIN --  MAX --");
    lv_obj_set_style_text_color(*range_label, lv_color_hex(0xA0A8B0), 0);
    lv_obj_set_style_text_font(*range_label, &lv_font_montserrat_20, 0);
    lv_obj_align(*range_label, LV_ALIGN_BOTTOM_MID, 0, -10);
    return value;
}

static void reset_stats_event(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        s_stats = (dashboard_stats_t){0};
    }
}

static void create_shiftlights(lv_obj_t *screen)
{
    const int diameter = 30;
    const int gap = 12;
    const int total_width = SHIFTLIGHT_COUNT * diameter +
                            (SHIFTLIGHT_COUNT - 1) * gap;
    const int start_x = (800 - total_width) / 2;

    for (int i = 0; i < SHIFTLIGHT_COUNT; ++i) {
        lv_obj_t *light = lv_obj_create(screen);
        lv_obj_remove_flag(light, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_pos(light, start_x + i * (diameter + gap), 7);
        lv_obj_set_size(light, diameter, diameter);
        lv_obj_set_style_radius(light, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(light, 2, 0);
        lv_obj_set_style_border_color(light, lv_color_hex(0x303840), 0);
        lv_obj_set_style_bg_color(light, lv_color_hex(0x101418), 0);
        lv_obj_set_style_bg_opa(light, LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(light, 0, 0);
        s_shiftlights[i] = light;
    }
}

static void update_shiftlights(uint16_t rpm)
{
    const int start = CONFIG_CAMARO_SHIFT_START_RPM;
    const int shift = CONFIG_CAMARO_SHIFT_RPM > start
                          ? CONFIG_CAMARO_SHIFT_RPM
                          : start + SHIFTLIGHT_COUNT;
    const bool flash_on = rpm < shift ||
                          ((lv_tick_get() / 125U) & 1U) == 0;

    for (int i = 0; i < SHIFTLIGHT_COUNT; ++i) {
        const int threshold = start +
                              ((shift - start) * i) / (SHIFTLIGHT_COUNT - 1);
        const bool active = rpm >= threshold && flash_on;
        lv_color_t color = lv_color_hex(0x101418);
        if (active) {
            color = i < 6 ? lv_color_hex(0x00FF40)
                          : (i < 9 ? lv_color_hex(0xFFE020)
                                   : lv_color_hex(0xFF2020));
        }
        lv_obj_set_style_bg_color(s_shiftlights[i], color, 0);
        lv_obj_set_style_border_color(s_shiftlights[i], active ? color
                                                               : lv_color_hex(0x303840),
                                      0);
    }
}

lv_obj_t *dashboard_classic_create(void)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "VEMS TELEMETRY DASHBOARD");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 42);

    create_shiftlights(screen);

    s_rpm_label = lv_label_create(screen);
    lv_label_set_text(s_rpm_label, "0");
    lv_obj_set_style_text_color(s_rpm_label, lv_color_hex(0x00FF40), 0);
    lv_obj_set_style_text_font(s_rpm_label, &lv_font_montserrat_48, 0);
    lv_obj_align(s_rpm_label, LV_ALIGN_TOP_MID, -45, 68);

    lv_obj_t *rpm_unit = lv_label_create(screen);
    lv_label_set_text(rpm_unit, "RPM");
    lv_obj_set_style_text_color(rpm_unit, lv_color_hex(0x00FF40), 0);
    lv_obj_set_style_text_font(rpm_unit, &lv_font_montserrat_28, 0);
    lv_obj_align(rpm_unit, LV_ALIGN_TOP_MID, 130, 82);

    s_rpm_max_label = lv_label_create(screen);
    lv_label_set_text(s_rpm_max_label, "MAX ----");
    lv_obj_set_style_text_color(s_rpm_max_label, lv_color_hex(0xA0A8B0), 0);
    lv_obj_set_style_text_font(s_rpm_max_label, &lv_font_montserrat_20, 0);
    lv_obj_align(s_rpm_max_label, LV_ALIGN_TOP_LEFT, 40, 108);

    s_rpm_bar = lv_bar_create(screen);
    lv_obj_set_pos(s_rpm_bar, 40, 138);
    lv_obj_set_size(s_rpm_bar, 720, 30);
    lv_bar_set_range(s_rpm_bar, 0, CONFIG_CAMARO_RPM_SCALE_MAX);
    lv_obj_set_style_bg_color(s_rpm_bar, lv_color_hex(0x202428), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_rpm_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_rpm_bar, lv_color_hex(0x00FF40), LV_PART_INDICATOR);

    s_map_label = create_value_box(screen, 40, "MAP (kPa)", lv_color_hex(0x00DFFF),
                                   &s_map_range_label);
    s_clt_label = create_value_box(screen, 290, "CLT (C)", lv_color_hex(0xFF9A20),
                                   &s_clt_range_label);
    s_lambda_label = create_value_box(screen, 540, "LAMBDA", lv_color_hex(0xFF40FF),
                                      &s_lambda_range_label);

    lv_obj_t *reset_button = lv_button_create(screen);
    lv_obj_set_size(reset_button, 190, 46);
    lv_obj_align(reset_button, LV_ALIGN_BOTTOM_MID, 0, -7);
    lv_obj_set_style_bg_color(reset_button, lv_color_hex(0x25303A), 0);
    lv_obj_add_event_cb(reset_button, reset_stats_event, LV_EVENT_CLICKED, NULL);

    lv_obj_t *reset_label = lv_label_create(reset_button);
    lv_label_set_text(reset_label, "RESET MIN/MAX");
    lv_obj_set_style_text_font(reset_label, &lv_font_montserrat_20, 0);
    lv_obj_center(reset_label);

    dashboard_add_switch_button(screen);

    const vems_data_t empty = {0};
    dashboard_classic_update(&empty);
    return screen;
}

void dashboard_classic_update(const vems_data_t *data)
{
    if (data == NULL || s_rpm_label == NULL) {
        return;
    }

    if (data->valid_mask & VEMS_VALID_RPM) {
        if (!s_stats.rpm_valid) {
            s_stats.rpm_min = data->rpm;
            s_stats.rpm_max = data->rpm;
            s_stats.rpm_valid = true;
        } else {
            if (data->rpm < s_stats.rpm_min) {
                s_stats.rpm_min = data->rpm;
            }
            if (data->rpm > s_stats.rpm_max) {
                s_stats.rpm_max = data->rpm;
            }
        }
        lv_label_set_text_fmt(s_rpm_label, "%u", (unsigned)data->rpm);
        lv_label_set_text_fmt(s_rpm_max_label, "MIN %u  MAX %u",
                              (unsigned)s_stats.rpm_min,
                              (unsigned)s_stats.rpm_max);
    } else {
        lv_label_set_text(s_rpm_label, "---");
        lv_label_set_text(s_rpm_max_label, "MIN ----  MAX ----");
    }

    if (data->valid_mask & VEMS_VALID_MAP) {
        if (!s_stats.map_valid) {
            s_stats.map_min = data->map_kpa;
            s_stats.map_max = data->map_kpa;
            s_stats.map_valid = true;
        } else {
            if (data->map_kpa < s_stats.map_min) {
                s_stats.map_min = data->map_kpa;
            }
            if (data->map_kpa > s_stats.map_max) {
                s_stats.map_max = data->map_kpa;
            }
        }
        lv_label_set_text_fmt(s_map_label, "%.1f", (double)data->map_kpa);
        lv_label_set_text_fmt(s_map_range_label, "MIN %.1f  MAX %.1f",
                              (double)s_stats.map_min, (double)s_stats.map_max);
    } else {
        lv_label_set_text(s_map_label, "--.-");
    }

    if (data->valid_mask & VEMS_VALID_CLT) {
        if (!s_stats.clt_valid) {
            s_stats.clt_min = data->coolant_c;
            s_stats.clt_max = data->coolant_c;
            s_stats.clt_valid = true;
        } else {
            if (data->coolant_c < s_stats.clt_min) {
                s_stats.clt_min = data->coolant_c;
            }
            if (data->coolant_c > s_stats.clt_max) {
                s_stats.clt_max = data->coolant_c;
            }
        }
        lv_label_set_text_fmt(s_clt_label, "%d", (int)data->coolant_c);
        lv_label_set_text_fmt(s_clt_range_label, "MIN %d  MAX %d",
                              (int)s_stats.clt_min, (int)s_stats.clt_max);
    } else {
        lv_label_set_text(s_clt_label, "---");
    }

    if (data->valid_mask & VEMS_VALID_LAMBDA) {
        if (!s_stats.lambda_valid) {
            s_stats.lambda_min = data->lambda;
            s_stats.lambda_max = data->lambda;
            s_stats.lambda_valid = true;
        } else {
            if (data->lambda < s_stats.lambda_min) {
                s_stats.lambda_min = data->lambda;
            }
            if (data->lambda > s_stats.lambda_max) {
                s_stats.lambda_max = data->lambda;
            }
        }
        lv_label_set_text_fmt(s_lambda_label, "%.2f", (double)data->lambda);
        lv_label_set_text_fmt(s_lambda_range_label, "MIN %.2f  MAX %.2f",
                              (double)s_stats.lambda_min,
                              (double)s_stats.lambda_max);
    } else {
        lv_label_set_text(s_lambda_label, "-.--");
    }

    const int rpm = data->rpm > CONFIG_CAMARO_RPM_SCALE_MAX
                        ? CONFIG_CAMARO_RPM_SCALE_MAX
                        : data->rpm;
    lv_bar_set_value(s_rpm_bar, rpm, LV_ANIM_OFF);
    update_shiftlights((data->valid_mask & VEMS_VALID_RPM) ? data->rpm : 0);
    const lv_color_t bar_color = data->rpm >= CONFIG_CAMARO_SHIFT_RPM
                                     ? lv_color_hex(0xFF2020)
                                     : (data->rpm >= CONFIG_CAMARO_SHIFT_START_RPM
                                            ? lv_color_hex(0xFFE020)
                                            : lv_color_hex(0x00FF40));
    lv_obj_set_style_bg_color(s_rpm_bar, bar_color, LV_PART_INDICATOR);
    lv_obj_set_style_text_color(s_clt_label,
                                data->coolant_c > CONFIG_CAMARO_CLT_WARNING_C
                                    ? lv_color_hex(0xFF2020)
                                    : lv_color_hex(0xFF9A20),
                                0);
}
