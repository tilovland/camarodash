#pragma once

#include "lvgl.h"
#include "vems.h"

/* Racepak/VEMS-style dashboard: arc tachometer, LED shiftlight bar and
 * flanking stat panels. Builds its own screen and returns it; does not
 * load it. */
lv_obj_t *dashboard_racepak_create(void);
void dashboard_racepak_update(const vems_data_t *data);
