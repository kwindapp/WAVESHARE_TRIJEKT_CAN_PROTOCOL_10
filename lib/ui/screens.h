#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <stdint.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Screen IDs (keep names expected by ui.c)
typedef enum ScreensEnum {
    SCREEN_ID_MAIN = 1
} ScreensEnum;

// All LVGL objects we reference from code
typedef struct _objects_t {
    lv_obj_t *main;

    // Gauge / RPM screen
    lv_obj_t *container_gauge;
    lv_obj_t *label_rpm;
    lv_obj_t *arc_rpm_0_40;
    lv_obj_t *bar_rpm_40_80;

    // Throttle + ECU data container
    lv_obj_t *container_throttle;
    lv_obj_t *label_throttle;
    lv_obj_t *label_temp;   
    // Battery + Lambda labels
    lv_obj_t *label_batt;
    lv_obj_t *label_lambda;
} objects_t;

// Defined in screens.c
extern objects_t objects;

// Theme colors from screens.c
extern uint32_t theme_colors[1][2];

// Screen creation / ticking API
void create_screens(void);
void tick_screen_main(void);
void tick_screen(int screen_index);
void tick_screen_by_id(enum ScreensEnum screenId);
void change_color_theme(uint32_t theme_index);

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/
