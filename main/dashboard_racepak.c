#include "dashboard_racepak.h"

#include <math.h>

#include "dashboard.h"

/*
 * Camaro 1977 / VEMS race display.
 * Card-framed stat panels, a graduated-color arc tachometer and a glowing
 * LED shiftlight bar, modeled on a reference mockup the driver supplied.
 */

#define LED_COUNT 10

#define TACH_CX            400
#define TACH_CY            240
#define TACH_R_RIM         190
#define TACH_R_BAND        174
#define TACH_BAND_WIDTH    22
#define TACH_R_LABEL       140
#define TACH_ANGLE_START   152.0f
#define TACH_ANGLE_SWEEP   236.0f

#define COL_CARD_FILL   lv_color_hex(0x0B0D10)
#define COL_CARD_BORDER lv_color_hex(0x262B33)
#define COL_CHIP_FILL   lv_color_hex(0x14181D)
#define COL_LABEL       lv_color_hex(0x8A93A0)
#define COL_MUTED       lv_color_hex(0x5C6874)
#define COL_TEXT        lv_color_hex(0xF2F3F4)
#define COL_RED         lv_color_hex(0xFF3B3B)
#define COL_BAR         lv_color_hex(0x2FC8FF)

typedef struct {
    lv_obj_t *value_label;
    lv_obj_t *bar;
} stat_panel_t;

typedef enum {
    ICON_RING,
    ICON_DROP,
    ICON_THERMO,
    ICON_BATTERY,
} icon_kind_t;

static lv_obj_t *s_led_core[LED_COUNT];
static lv_obj_t *s_led_halo[LED_COUNT];
static lv_obj_t *s_rpm_value;
static lv_obj_t *s_needle_scale;
static lv_obj_t *s_needle_line;
static stat_panel_t s_boost;
static stat_panel_t s_coolant;
static stat_panel_t s_afr;

static float value_to_angle(float v)
{
    return TACH_ANGLE_START + (v / 8.0f) * TACH_ANGLE_SWEEP;
}

static void polar_xy(float angle_deg, float radius, int *x, int *y)
{
    const float rad = angle_deg * 0.0174532925f;
    *x = TACH_CX + (int)lroundf(radius * cosf(rad));
    *y = TACH_CY + (int)lroundf(radius * sinf(rad));
}

/* Plain filled rectangle/circle building block for icons and hairlines. */
static lv_obj_t *shape(lv_obj_t *parent, int w, int h, lv_color_t color)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_color(obj, color, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    return obj;
}

static lv_obj_t *make_card(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_size(card, w, h);
    lv_obj_set_style_bg_color(card, COL_CARD_FILL, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card, COL_CARD_BORDER, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    return card;
}

/* Small geometric stand-ins for a channel icon: a ring (boost/AFR gauges), a
 * drop (oil pressure), a thermometer (coolant/intake air) and a battery.
 * They are plain shapes, not artwork -- swap in bitmap icons later via
 * esp_mmap_assets if a closer match is wanted. */
static lv_obj_t *make_icon(lv_obj_t *parent, icon_kind_t kind, lv_color_t color)
{
    lv_obj_t *box = lv_obj_create(parent);
    lv_obj_remove_style_all(box);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(box, 22, 22);

    switch (kind) {
    case ICON_RING: {
        lv_obj_t *ring = shape(box, 18, 18, color);
        lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
        lv_obj_center(ring);
        lv_obj_t *hole = shape(ring, 8, 8, COL_CHIP_FILL);
        lv_obj_set_style_radius(hole, LV_RADIUS_CIRCLE, 0);
        lv_obj_center(hole);
        break;
    }
    case ICON_DROP: {
        lv_obj_t *bulb = shape(box, 14, 14, color);
        lv_obj_set_style_radius(bulb, LV_RADIUS_CIRCLE, 0);
        lv_obj_align(bulb, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_t *tip = shape(box, 10, 10, color);
        lv_obj_set_style_transform_rotation(tip, 450, 0);
        lv_obj_align(tip, LV_ALIGN_TOP_MID, 0, 3);
        break;
    }
    case ICON_THERMO: {
        lv_obj_t *stem = shape(box, 6, 14, color);
        lv_obj_set_style_radius(stem, 3, 0);
        lv_obj_align(stem, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_t *bulb = shape(box, 12, 12, color);
        lv_obj_set_style_radius(bulb, LV_RADIUS_CIRCLE, 0);
        lv_obj_align(bulb, LV_ALIGN_BOTTOM_MID, 0, 0);
        break;
    }
    case ICON_BATTERY: {
        lv_obj_t *body = lv_obj_create(box);
        lv_obj_remove_style_all(body);
        lv_obj_remove_flag(body, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(body, 17, 12);
        lv_obj_set_style_radius(body, 2, 0);
        lv_obj_set_style_border_width(body, 2, 0);
        lv_obj_set_style_border_color(body, color, 0);
        lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
        lv_obj_align(body, LV_ALIGN_LEFT_MID, 0, 0);
        lv_obj_t *nub = shape(box, 2, 6, color);
        lv_obj_align(nub, LV_ALIGN_RIGHT_MID, 0, 0);
        break;
    }
    }
    return box;
}

/* One card: icon chip + label / big value + unit on row 1, min/bar/max on
 * row 2. Coordinates are local to the card. */
static stat_panel_t make_stat_panel(lv_obj_t *parent, int x, int y, int w,
                                    icon_kind_t icon, lv_color_t color,
                                    const char *label, const char *unit,
                                    const char *min_text, const char *max_text,
                                    int32_t bar_min, int32_t bar_max)
{
    const int h = 104;
    lv_obj_t *card = make_card(parent, x, y, w, h);
    stat_panel_t panel = {0};

    /* Row 1 (top): icon chip + small label. */
    lv_obj_t *chip = lv_obj_create(card);
    lv_obj_remove_flag(chip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(chip, 14, 12);
    lv_obj_set_size(chip, 28, 28);
    lv_obj_set_style_bg_color(chip, COL_CHIP_FILL, 0);
    lv_obj_set_style_bg_opa(chip, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(chip, 0, 0);
    lv_obj_set_style_radius(chip, 7, 0);
    lv_obj_t *icon_obj = make_icon(chip, icon, color);
    lv_obj_center(icon_obj);

    lv_obj_t *label_obj = lv_label_create(card);
    lv_label_set_text(label_obj, label);
    lv_obj_set_style_text_color(label_obj, COL_LABEL, 0);
    lv_obj_set_style_text_font(label_obj, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(label_obj, 50, 18);

    /* Row 2 (middle): big value + unit, centered as a group. */
    lv_obj_t *value_row = lv_obj_create(card);
    lv_obj_remove_style_all(value_row);
    lv_obj_remove_flag(value_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(value_row, 0, 44);
    lv_obj_set_size(value_row, w, 36);
    lv_obj_set_flex_flow(value_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(value_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(value_row, 5, 0);

    panel.value_label = lv_label_create(value_row);
    lv_label_set_text(panel.value_label, "--");
    lv_obj_set_style_text_color(panel.value_label, color, 0);
    lv_obj_set_style_text_font(panel.value_label, &lv_font_montserrat_28, 0);

    lv_obj_t *unit_obj = lv_label_create(value_row);
    lv_label_set_text(unit_obj, unit);
    lv_obj_set_style_text_color(unit_obj, COL_MUTED, 0);
    lv_obj_set_style_text_font(unit_obj, &lv_font_montserrat_14, 0);

    /* Row 3 (bottom): min label, bar, max label. */
    lv_obj_t *min_label = lv_label_create(card);
    lv_label_set_text(min_label, min_text);
    lv_obj_set_style_text_color(min_label, COL_MUTED, 0);
    lv_obj_set_style_text_font(min_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(min_label, 14, 82);

    lv_obj_t *max_label = lv_label_create(card);
    lv_label_set_text(max_label, max_text);
    lv_obj_set_style_text_color(max_label, COL_MUTED, 0);
    lv_obj_set_style_text_font(max_label, &lv_font_montserrat_14, 0);
    lv_obj_set_width(max_label, 24);
    lv_obj_set_style_text_align(max_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_pos(max_label, w - 38, 82);

    panel.bar = lv_bar_create(card);
    lv_obj_set_pos(panel.bar, 38, 84);
    lv_obj_set_size(panel.bar, w - 76, 6);
    lv_bar_set_range(panel.bar, bar_min, bar_max);
    lv_bar_set_value(panel.bar, bar_min, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(panel.bar, lv_color_hex(0x181C22), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(panel.bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(panel.bar, COL_BAR, LV_PART_INDICATOR);
    lv_obj_set_style_radius(panel.bar, 3, LV_PART_MAIN);
    lv_obj_set_style_radius(panel.bar, 3, LV_PART_INDICATOR);

    return panel;
}

/* One solid-colored slice of the tachometer's graduated ring. */
static void add_tach_zone(lv_obj_t *parent, float from_v, float to_v, lv_color_t color)
{
    lv_obj_t *arc = lv_arc_create(parent);
    lv_obj_set_size(arc, TACH_R_BAND * 2, TACH_R_BAND * 2);
    lv_obj_align(arc, LV_ALIGN_CENTER, 0, 0);
    lv_arc_set_bg_angles(arc, value_to_angle(from_v), fmodf(value_to_angle(to_v), 360.0f));
    lv_arc_set_range(arc, 0, 1000);
    lv_arc_set_value(arc, 1000);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_opa(arc, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, TACH_BAND_WIDTH, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, color, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, false, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
}

static void build_tachometer(lv_obj_t *screen)
{
    const float start_v = CONFIG_CAMARO_SHIFT_START_RPM / 1000.0f;
    const float shift_v = CONFIG_CAMARO_SHIFT_RPM / 1000.0f;
    const float span = shift_v - start_v;

    add_tach_zone(screen, 0.0f, start_v, lv_color_hex(0x3A3D42));
    add_tach_zone(screen, start_v, start_v + span * 0.33f, lv_color_hex(0xFFD24D));
    add_tach_zone(screen, start_v + span * 0.33f, start_v + span * 0.66f, lv_color_hex(0xFFB629));
    add_tach_zone(screen, start_v + span * 0.66f, shift_v, lv_color_hex(0xFF8A3D));
    add_tach_zone(screen, shift_v, 8.0f, COL_RED);

    /* Thin outer rim. */
    lv_obj_t *rim = lv_arc_create(screen);
    lv_obj_set_size(rim, TACH_R_RIM * 2, TACH_R_RIM * 2);
    lv_obj_align(rim, LV_ALIGN_CENTER, 0, 0);
    lv_arc_set_bg_angles(rim, TACH_ANGLE_START, fmodf(TACH_ANGLE_START + TACH_ANGLE_SWEEP, 360.0f));
    lv_obj_remove_style(rim, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_opa(rim, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(rim, 3, LV_PART_MAIN);
    lv_obj_set_style_arc_color(rim, lv_color_hex(0xC7CDD4), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(rim, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(rim, LV_OBJ_FLAG_CLICKABLE);

    /* Tick numbers, computed around the ring rather than hand-placed. */
    for (int v = 0; v <= 8; ++v) {
        int px, py;
        polar_xy(value_to_angle((float)v), TACH_R_LABEL, &px, &py);

        lv_obj_t *lbl = lv_label_create(screen);
        lv_label_set_text_fmt(lbl, "%d", v);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_28, 0);
        lv_obj_set_style_text_color(lbl, v == 8 ? COL_RED : COL_TEXT, 0);
        lv_obj_set_width(lbl, 32);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(lbl, px - 16, py - 15);
    }

    /* Needle: a hidden lv_scale drives the rotation math via
     * lv_scale_set_line_needle_value(), same geometry as the visible ring. */
    s_needle_scale = lv_scale_create(screen);
    lv_obj_set_size(s_needle_scale, TACH_R_RIM * 2, TACH_R_RIM * 2);
    lv_obj_align(s_needle_scale, LV_ALIGN_CENTER, 0, 0);
    lv_scale_set_mode(s_needle_scale, LV_SCALE_MODE_ROUND_INNER);
    lv_scale_set_range(s_needle_scale, 0, CONFIG_CAMARO_RPM_SCALE_MAX);
    lv_scale_set_angle_range(s_needle_scale, (uint32_t)TACH_ANGLE_SWEEP);
    lv_scale_set_rotation(s_needle_scale, (int32_t)TACH_ANGLE_START);
    lv_scale_set_label_show(s_needle_scale, false);
    lv_obj_set_style_bg_opa(s_needle_scale, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_needle_scale, 0, 0);
    lv_obj_set_style_line_opa(s_needle_scale, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_line_opa(s_needle_scale, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_style_line_opa(s_needle_scale, LV_OPA_TRANSP, LV_PART_ITEMS);
    lv_obj_remove_flag(s_needle_scale, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(s_needle_scale, LV_OBJ_FLAG_CLICKABLE);

    s_needle_line = lv_line_create(s_needle_scale);
    lv_obj_set_style_line_color(s_needle_line, COL_RED, LV_PART_MAIN);
    lv_obj_set_style_line_width(s_needle_line, 5, LV_PART_MAIN);
    lv_obj_set_style_line_rounded(s_needle_line, true, LV_PART_MAIN);
    lv_scale_set_line_needle_value(s_needle_scale, s_needle_line, TACH_R_LABEL + 20, 0);

    /* Hub face: hides the needle's inner run and the pivot point so only
     * the outer portion reads as a needle, like a real gauge's center cap. */
    lv_obj_t *hub = lv_obj_create(screen);
    lv_obj_remove_style_all(hub);
    lv_obj_remove_flag(hub, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(hub, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(hub, 196, 196);
    lv_obj_align(hub, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(hub, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(hub, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(hub, LV_OPA_COVER, 0);
}

static void create_shift_lights(lv_obj_t *parent)
{
    const int d = 20, gap = 12;
    const int total = LED_COUNT * d + (LED_COUNT - 1) * gap;
    const int start_x = TACH_CX - total / 2;
    const int y = 26;

    for (int i = 0; i < LED_COUNT; ++i) {
        const int cx = start_x + i * (d + gap) + d / 2;

        lv_obj_t *halo = lv_obj_create(parent);
        lv_obj_remove_style_all(halo);
        lv_obj_remove_flag(halo, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(halo, d + 14, d + 14);
        lv_obj_set_style_radius(halo, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(halo, LV_OPA_TRANSP, 0);
        lv_obj_set_pos(halo, cx - (d + 14) / 2, y - 7);
        s_led_halo[i] = halo;

        lv_obj_t *core = lv_obj_create(parent);
        lv_obj_remove_style_all(core);
        lv_obj_remove_flag(core, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(core, d, d);
        lv_obj_set_style_radius(core, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(core, lv_color_hex(0x24272C), 0);
        lv_obj_set_style_bg_opa(core, LV_OPA_COVER, 0);
        lv_obj_set_pos(core, cx - d / 2, y);
        s_led_core[i] = core;
    }
}

static void create_header(lv_obj_t *parent)
{
    lv_obj_t *camaro = lv_label_create(parent);
    lv_label_set_text(camaro, "CAMARO");
    lv_obj_set_style_text_color(camaro, COL_TEXT, 0);
    lv_obj_set_style_text_font(camaro, &lv_font_montserrat_28, 0);
    lv_obj_set_pos(camaro, 28, 18);

    lv_obj_t *year = lv_label_create(parent);
    lv_label_set_text(year, "1977");
    lv_obj_set_style_text_color(year, COL_RED, 0);
    lv_obj_set_style_text_font(year, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(year, 29, 52);

    lv_obj_t *vems = lv_label_create(parent);
    lv_label_set_text(vems, "VEMS");
    lv_obj_set_style_text_color(vems, COL_RED, 0);
    lv_obj_set_style_text_font(vems, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_align(vems, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_width(vems, 160);
    lv_obj_set_pos(vems, 612, 18);

    lv_obj_t *tagline = lv_label_create(parent);
    lv_label_set_text(tagline, "ENGINE MANAGEMENT");
    lv_obj_set_style_text_color(tagline, COL_MUTED, 0);
    lv_obj_set_style_text_font(tagline, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(tagline, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_width(tagline, 160);
    lv_obj_set_pos(tagline, 612, 52);
}

static void update_shift_lights(uint16_t rpm)
{
    const int start = CONFIG_CAMARO_SHIFT_START_RPM;
    const int shift = CONFIG_CAMARO_SHIFT_RPM > start
                          ? CONFIG_CAMARO_SHIFT_RPM
                          : start + LED_COUNT;
    const bool flash_on = rpm < shift || ((lv_tick_get() / 125U) & 1U) == 0;

    for (int i = 0; i < LED_COUNT; ++i) {
        const int threshold = start + ((shift - start) * i) / (LED_COUNT - 1);
        const bool active = rpm >= (uint16_t)threshold && flash_on;
        const lv_color_t color = i < 4 ? lv_color_hex(0x2BE07A)
                                       : (i < 5 ? lv_color_hex(0xFFD24D)
                                                : (i < 7 ? lv_color_hex(0xFF8A1E)
                                                         : COL_RED));
        if (active) {
            lv_obj_set_style_bg_color(s_led_core[i], color, 0);
            lv_obj_set_style_bg_color(s_led_halo[i], color, 0);
            lv_obj_set_style_bg_opa(s_led_halo[i], LV_OPA_40, 0);
        } else {
            lv_obj_set_style_bg_color(s_led_core[i], lv_color_hex(0x24272C), 0);
            lv_obj_set_style_bg_opa(s_led_halo[i], LV_OPA_TRANSP, 0);
        }
    }
}

lv_obj_t *dashboard_racepak_create(void)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    create_header(screen);
    create_shift_lights(screen);

    /* Left column: boost (from MAP), oil pressure (no sensor yet), coolant. */
    s_boost = make_stat_panel(screen, 24, 92, 180, ICON_RING, lv_color_hex(0x2FC8FF),
                              "BOOST", "bar", "0", "2.5", 0, 250);
    make_stat_panel(screen, 24, 210, 180, ICON_DROP, lv_color_hex(0x3A4048),
                    "OIL PRESS", "bar", "0", "10", 0, 100);
    s_coolant = make_stat_panel(screen, 24, 328, 180, ICON_THERMO, lv_color_hex(0xFFA03C),
                                "COOLANT", "\xC2\xB0" "C", "40", "120", 40, 120);

    /* Right column: AFR (derived from lambda), battery and intake air (no
     * sensors yet). */
    s_afr = make_stat_panel(screen, 596, 92, 180, ICON_RING, lv_color_hex(0xFF57F5),
                            "AFR", "", "10", "18", 100, 180);
    make_stat_panel(screen, 596, 210, 180, ICON_BATTERY, lv_color_hex(0x3A4048),
                    "BATTERY", "V", "10", "16", 0, 150);
    make_stat_panel(screen, 596, 328, 180, ICON_THERMO, lv_color_hex(0x3A4048),
                    "INTAKE AIR", "\xC2\xB0" "C", "-20", "60", 0, 60);

    build_tachometer(screen);

    /* Digital RPM cluster, centered on the tachometer. */
    lv_obj_t *rpm_caption = lv_label_create(screen);
    lv_label_set_text(rpm_caption, "RPM");
    lv_obj_set_style_text_color(rpm_caption, COL_LABEL, 0);
    lv_obj_set_style_text_font(rpm_caption, &lv_font_montserrat_20, 0);
    lv_obj_align(rpm_caption, LV_ALIGN_CENTER, 0, -46);

    s_rpm_value = lv_label_create(screen);
    lv_label_set_text(s_rpm_value, "0");
    lv_obj_set_style_text_color(s_rpm_value, COL_TEXT, 0);
    lv_obj_set_style_text_font(s_rpm_value, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_align(s_rpm_value, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(s_rpm_value, 220);
    lv_obj_align(s_rpm_value, LV_ALIGN_CENTER, 0, -6);

    lv_obj_t *x1000_caption = lv_label_create(screen);
    lv_label_set_text(x1000_caption, "x1000");
    lv_obj_set_style_text_color(x1000_caption, COL_LABEL, 0);
    lv_obj_set_style_text_font(x1000_caption, &lv_font_montserrat_20, 0);
    lv_obj_align(x1000_caption, LV_ALIGN_CENTER, 0, 44);

    /* Speed: no VEMS channel decoded for it yet, so it stays a dimmed
     * placeholder until the AIM parser learns that channel byte. Centered
     * under the tachometer now that the gear box is gone. */
    const int speed_w = 260;
    lv_obj_t *speed_card = make_card(screen, TACH_CX - speed_w / 2, 372, speed_w, 76);

    lv_obj_t *speed_caption = lv_label_create(speed_card);
    lv_label_set_text(speed_caption, "SPEED");
    lv_obj_set_style_text_color(speed_caption, COL_LABEL, 0);
    lv_obj_set_style_text_font(speed_caption, &lv_font_montserrat_20, 0);
    lv_obj_set_width(speed_caption, speed_w);
    lv_obj_set_style_text_align(speed_caption, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(speed_caption, 0, 8);

    lv_obj_t *speed_row = lv_obj_create(speed_card);
    lv_obj_remove_style_all(speed_row);
    lv_obj_remove_flag(speed_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(speed_row, 0, 34);
    lv_obj_set_size(speed_row, speed_w, 32);
    lv_obj_set_flex_flow(speed_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(speed_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(speed_row, 6, 0);

    lv_obj_t *speed_value = lv_label_create(speed_row);
    lv_label_set_text(speed_value, "--");
    lv_obj_set_style_text_color(speed_value, lv_color_hex(0x4A5560), 0);
    lv_obj_set_style_text_font(speed_value, &lv_font_montserrat_28, 0);

    lv_obj_t *speed_unit = lv_label_create(speed_row);
    lv_label_set_text(speed_unit, "km/h");
    lv_obj_set_style_text_color(speed_unit, COL_MUTED, 0);
    lv_obj_set_style_text_font(speed_unit, &lv_font_montserrat_14, 0);

    /* Footer. */
    lv_obj_t *tuned_label = lv_label_create(screen);
    lv_label_set_text(tuned_label, "TUNED BY VEMS");
    lv_obj_set_style_text_color(tuned_label, COL_MUTED, 0);
    lv_obj_set_style_text_font(tuned_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(tuned_label, 28, 458);

    lv_obj_t *built_label = lv_label_create(screen);
    lv_label_set_text(built_label, "BUILT TO DRIVE");
    lv_obj_set_style_text_color(built_label, COL_MUTED, 0);
    lv_obj_set_style_text_font(built_label, &lv_font_montserrat_14, 0);
    lv_obj_align(built_label, LV_ALIGN_BOTTOM_RIGHT, -66, -22);

    dashboard_add_switch_button(screen);

    const vems_data_t empty = {0};
    dashboard_racepak_update(&empty);
    return screen;
}

void dashboard_racepak_update(const vems_data_t *data)
{
    if (data == NULL || s_rpm_value == NULL) {
        return;
    }

    const uint16_t rpm = (data->valid_mask & VEMS_VALID_RPM) ? data->rpm : 0;
    lv_label_set_text_fmt(s_rpm_value, "%u", (unsigned)rpm);
    lv_obj_set_style_text_color(s_rpm_value,
                                rpm >= CONFIG_CAMARO_SHIFT_RPM ? COL_RED : COL_TEXT, 0);
    lv_scale_set_line_needle_value(s_needle_scale, s_needle_line, TACH_R_LABEL + 20,
                                   rpm > CONFIG_CAMARO_RPM_SCALE_MAX ? CONFIG_CAMARO_RPM_SCALE_MAX
                                                                      : rpm);

    if (data->valid_mask & VEMS_VALID_MAP) {
        lv_label_set_text_fmt(s_boost.value_label, "%.2f", (double)(data->map_kpa / 100.0f));
        lv_bar_set_value(s_boost.bar, (int32_t)data->map_kpa, LV_ANIM_OFF);
    } else {
        lv_label_set_text(s_boost.value_label, "--");
        lv_bar_set_value(s_boost.bar, 0, LV_ANIM_OFF);
    }

    if (data->valid_mask & VEMS_VALID_CLT) {
        lv_label_set_text_fmt(s_coolant.value_label, "%d", (int)data->coolant_c);
        lv_obj_set_style_text_color(s_coolant.value_label,
                                    data->coolant_c > CONFIG_CAMARO_CLT_WARNING_C
                                        ? COL_RED
                                        : lv_color_hex(0xFFA03C),
                                    0);
        lv_bar_set_value(s_coolant.bar, data->coolant_c, LV_ANIM_OFF);
    } else {
        lv_label_set_text(s_coolant.value_label, "--");
        lv_bar_set_value(s_coolant.bar, 40, LV_ANIM_OFF);
    }

    if (data->valid_mask & VEMS_VALID_LAMBDA) {
        const float afr = data->lambda * 14.7f;
        lv_label_set_text_fmt(s_afr.value_label, "%.1f", (double)afr);
        lv_bar_set_value(s_afr.bar, (int32_t)(afr * 10.0f), LV_ANIM_OFF);
    } else {
        lv_label_set_text(s_afr.value_label, "--");
        lv_bar_set_value(s_afr.bar, 100, LV_ANIM_OFF);
    }

    update_shift_lights(rpm);
}
