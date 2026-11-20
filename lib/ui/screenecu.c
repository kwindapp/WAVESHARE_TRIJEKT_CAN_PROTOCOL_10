#include "screenecu.h"
#include <stdint.h>

extern void open_trijekt_mode1(void);

#define ECU_COUNT 20
static lv_obj_t *screen = NULL;
static lv_obj_t *values[ECU_COUNT] = {0};
static const char *names[ECU_COUNT] = {
    "RPM", "Torque", "Status", "Fuel", "Throttle", "Pedal", "E-Gas", "E-Target", "Engine Temp", "Air Temp",
    "ECU Temp", "Oil Temp", "Intake", "External", "Fuel Press", "Oil Press", "Lambda 1", "Lambda 2", "Battery", "Max RPM"
};

static void dashboard_clicked(lv_event_t *event) {
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        open_trijekt_mode1();
    }
}

lv_obj_t *screenecu_create(lv_obj_t *parent) {
    if (screen) return screen;
    screen = lv_obj_create(parent);
    lv_obj_set_size(screen, 800, 480);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x080B10), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *header = lv_label_create(screen);
    lv_label_set_text(header, "TRIJEKT  |  ECU DATA");
    lv_obj_set_style_text_color(header, lv_color_hex(0xFFAA44), 0);
    lv_obj_set_pos(header, 20, 19);

    lv_obj_t *back = lv_btn_create(screen);
    lv_obj_set_size(back, 170, 54);
    lv_obj_set_pos(back, 610, 8);
    lv_obj_set_style_bg_color(back, lv_color_hex(0xA64C1B), 0);
    lv_obj_clear_flag(back, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(back, dashboard_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_t *txt = lv_label_create(back);
    lv_label_set_text(txt, "< DASHBOARD");
    lv_obj_center(txt);

    for (int i = 0; i < ECU_COUNT; ++i) {
        int col = i / 10;
        int row = i % 10;
        lv_obj_t *l = lv_label_create(screen);
        values[i] = l;
        lv_obj_set_pos(l, 20 + col * 400, 78 + row * 38);
        lv_obj_set_width(l, 380);
        lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_color(l, lv_color_hex(0xF1F3F6), 0);
        lv_label_set_text_fmt(l, "%s: ---", names[i]);
    }
    lv_obj_add_flag(screen, LV_OBJ_FLAG_HIDDEN);
    return screen;
}

lv_obj_t *screenecu_value_label(int index) {
    if (index < 0 || index >= ECU_COUNT) return NULL;
    return values[index];
}
