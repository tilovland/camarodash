#include "dashboard.h"

#include <stddef.h>

#include "dashboard_classic.h"
#include "dashboard_racepak.h"

#define DASHBOARD_COUNT 2

typedef struct {
    lv_obj_t *screen;
    void (*update)(const vems_data_t *data);
} dashboard_entry_t;

static dashboard_entry_t s_dashboards[DASHBOARD_COUNT];
static size_t s_active;

static void switch_button_event_cb(lv_event_t *event)
{
    (void)event;
    dashboard_manager_next();
}

void dashboard_add_switch_button(lv_obj_t *screen)
{
    lv_obj_t *button = lv_button_create(screen);
    lv_obj_set_size(button, 44, 44);
    lv_obj_align(button, LV_ALIGN_BOTTOM_RIGHT, -8, -8);
    lv_obj_set_style_radius(button, 22, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x1A1D22), 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_70, 0);
    lv_obj_set_style_border_width(button, 1, 0);
    lv_obj_set_style_border_color(button, lv_color_hex(0x3A4048), 0);
    lv_obj_add_event_cb(button, switch_button_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, LV_SYMBOL_NEXT);
    lv_obj_center(label);
}

esp_err_t dashboard_manager_init(void)
{
    s_dashboards[0].screen = dashboard_classic_create();
    s_dashboards[0].update = dashboard_classic_update;
    s_dashboards[1].screen = dashboard_racepak_create();
    s_dashboards[1].update = dashboard_racepak_update;

    for (size_t i = 0; i < DASHBOARD_COUNT; ++i) {
        if (s_dashboards[i].screen == NULL) {
            return ESP_FAIL;
        }
    }

    s_active = 0;
    lv_screen_load(s_dashboards[s_active].screen);
    return ESP_OK;
}

void dashboard_manager_update(const vems_data_t *data)
{
    if (s_dashboards[s_active].update != NULL) {
        s_dashboards[s_active].update(data);
    }
}

void dashboard_manager_next(void)
{
    s_active = (s_active + 1) % DASHBOARD_COUNT;
    lv_screen_load_anim(s_dashboards[s_active].screen, LV_SCREEN_LOAD_ANIM_MOVE_LEFT,
                        220, 0, false);
}
