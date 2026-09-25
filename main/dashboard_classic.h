#pragma once

#include "lvgl.h"
#include "vems.h"

/* The original ring/box telemetry dashboard. Builds its own screen and
 * returns it; does not load it. */
lv_obj_t *dashboard_classic_create(void);
void dashboard_classic_update(const vems_data_t *data);
