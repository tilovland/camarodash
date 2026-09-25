#pragma once

#include "esp_err.h"
#include "lvgl.h"
#include "vems.h"

/* Builds every registered dashboard screen and shows the first one. */
esp_err_t dashboard_manager_init(void);

/* Refreshes the currently visible dashboard with a new telemetry sample. */
void dashboard_manager_update(const vems_data_t *data);

/* Switches to the next registered dashboard, wrapping around. */
void dashboard_manager_next(void);

/* Shared touch control every dashboard screen adds to let the driver cycle
 * to the next dashboard style. */
void dashboard_add_switch_button(lv_obj_t *screen);
