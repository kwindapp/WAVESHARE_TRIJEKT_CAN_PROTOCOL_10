#include <string.h>

#include "screens.h"
#include "images.h"
#include "fonts.h"
#include "actions.h"
#include "vars.h"
#include "styles.h"
#include "ui.h"

#include <string.h>

objects_t objects;
lv_obj_t *tick_value_change_obj;
uint32_t active_theme_index = 0;

// -------------------------------------------------------------------
// Internal button state (for future CAN-marker logic)
// -------------------------------------------------------------------
static int marker_left_active  = 0;
static int marker_right_active = 0;

// Getter so other modules (e.g. main.cpp) can query Mode 2 state
int get_marker_right_state(void) {
    return marker_right_active;
}

// TEST_MODE is defined in your main file; we control it from here
extern bool TEST_MODE;

// -------------------------------------------------------------------
// Color configuration (easy to tweak)
// -------------------------------------------------------------------
#define UI_COLOR_TITLE      lv_color_hex(0xA4ADB3)  // top title
#define UI_COLOR_RPM_TEXT   lv_color_hex(0xD5DAE0)  // big RPM number
#define UI_COLOR_SCALE_TEXT lv_color_hex(0xD1DDE3)  // scale 0..8 labels
#define UI_COLOR_INFO_TEXT  lv_color_hex(0xA4ADB3)  // Batt / Lambda / buttons

// Fancy green for RPM arc (0x00FF00)
#define UI_RPM_ARC_COLOR    lv_color_hex(0x24A624)

// -------------------------------------------------------------------
// Layout constants (easy to tweak positions / sizes)
// -------------------------------------------------------------------
static const int UI_SCREEN_W = 800;
static const int UI_SCREEN_H = 480;

// Title
static const int UI_TITLE_Y = 5;

// Gauge container
static const int UI_GAUGE_X = 40;
static const int UI_GAUGE_Y = 20;
static const int UI_GAUGE_W = 760;
static const int UI_GAUGE_H = 350;

// RPM big label
static const int UI_RPM_LABEL_X = 150;
static const int UI_RPM_LABEL_Y = 98;
static const int UI_RPM_LABEL_W = 491;

// RPM arc
static const int UI_ARC_X = 0;
static const int UI_ARC_Y = 0;
static const int UI_ARC_SIZE = 350;

// RPM bar (4–8k)
static const int UI_BAR_X = 175;
static const int UI_BAR_Y = 0;
static const int UI_BAR_W = 485;
static const int UI_BAR_H = 65; // bar height

// Scale text positions around arc
static const int UI_SCALE_0_X = 160;
static const int UI_SCALE_0_Y = 275;
static const int UI_SCALE_1_X = 73;
static const int UI_SCALE_1_Y = 238;
static const int UI_SCALE_2_X = 20;
static const int UI_SCALE_2_Y = 155;
static const int UI_SCALE_3_X = 45;
static const int UI_SCALE_3_Y = 50;
static const int UI_SCALE_4_X = 130;
static const int UI_SCALE_4_Y = -10;
static const int UI_SCALE_5_X = 259;
static const int UI_SCALE_5_Y = -15;
static const int UI_SCALE_6_X = 380;
static const int UI_SCALE_6_Y = -15;
static const int UI_SCALE_7_X = 503;
static const int UI_SCALE_7_Y = -15;
static const int UI_SCALE_8_X = 613;
static const int UI_SCALE_8_Y = -15;

// ECU info container (Batt + Lambda) – centered bottom
static const int UI_INFO_W = 450;
static const int UI_INFO_H = 165;
static const int UI_INFO_X = (UI_SCREEN_W - UI_INFO_W) / 2; // center horizontally
static const int UI_INFO_Y = 340;                            // near bottom

// Batt / Lambda positions inside the info container (left side)
static const int UI_BATT_LABEL_X   = 80;
static const int UI_BATT_LABEL_Y   = -20;
static const int UI_LAMBDA_LABEL_X = 80;
static const int UI_LAMBDA_LABEL_Y = 40;

// Bottom marker buttons
static const int UI_MARKER_BTN_W = 120;
static const int UI_MARKER_BTN_H = 60;
static const int UI_MARKER_BTN_Y = UI_SCREEN_H - UI_MARKER_BTN_H - 20;
static const int UI_MARKER_LEFT_X  = 30;
static const int UI_MARKER_RIGHT_X = UI_SCREEN_W - UI_MARKER_BTN_W - 30;

// Temp label above Mode 2 button
static const int UI_THR_ABOVE_X  = UI_MARKER_RIGHT_X + 50; // near right side
static const int UI_THR_ABOVE_Y  = UI_MARKER_BTN_Y - 50;   // above button

// -------------------------------------------------------------------
// Marker button style helper
// -------------------------------------------------------------------
static lv_obj_t *btn_marker_left  = NULL;
static lv_obj_t *btn_marker_right = NULL;

static void marker_btn_update_style(lv_obj_t *btn, int active) {
    if (!btn) return;

    lv_color_t bg = active ? lv_color_hex(0x006400)  // dark green
                           : lv_color_hex(0x303030); // dark grey

    lv_obj_set_style_bg_color(btn, bg, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(btn, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(btn, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(btn, lv_color_hex(0xFFFFFF),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
}

// -------------------------------------------------------------------
// Marker button callbacks
// -------------------------------------------------------------------
static void marker_left_btn_event_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        marker_left_active = !marker_left_active;
        marker_btn_update_style(btn_marker_left, marker_left_active);
    }
}

static void marker_right_btn_event_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        // toggle UI state
        marker_right_active = !marker_right_active;
        marker_btn_update_style(btn_marker_right, marker_right_active);

        // link Mode 2 button to TEST_MODE
        TEST_MODE = (marker_right_active != 0);
    }
}

void create_screen_main() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.main = obj;
    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][1]),
                              LV_PART_MAIN | LV_STATE_DEFAULT);

    {
        lv_obj_t *parent_obj = obj;

        // --------------------------------------------------
        // Top title label
        // --------------------------------------------------
        {
            lv_obj_t *title = lv_label_create(parent_obj);
            lv_label_set_text(title, "RWB JANINE DASH");
            lv_obj_set_style_text_font(title, &ui_font_Hollow85,
                                       LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(title,
                                        UI_COLOR_TITLE,
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_align(title, LV_ALIGN_TOP_MID, 0, UI_TITLE_Y);

            lv_obj_t *title1 = lv_label_create(parent_obj);
            lv_label_set_text(title1, "RAUH Welt BEGRIFF");
            lv_obj_set_style_text_font(title1, &ui_font_t20,
                                       LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(title1,
                                        UI_COLOR_TITLE,
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_align(title1, LV_ALIGN_TOP_MID, 0, UI_TITLE_Y + 150);
        }

        // --------------------------------------------------
        // container gauge
        // --------------------------------------------------
        {
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.container_gauge = obj;
            lv_obj_set_pos(obj, UI_GAUGE_X, UI_GAUGE_Y);
            lv_obj_set_size(obj, UI_GAUGE_W, UI_GAUGE_H);
            lv_obj_set_style_pad_left(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_top(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_right(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_pad_bottom(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);

            {
                lv_obj_t *parent_obj = obj;

                // ------------------------
                // label rpm (big number)
                // ------------------------
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.label_rpm = obj;
                    lv_obj_set_pos(obj, UI_RPM_LABEL_X, UI_RPM_LABEL_Y);
                    lv_obj_set_size(obj, UI_RPM_LABEL_W, LV_SIZE_CONTENT);
                    lv_obj_set_style_text_font(obj, &ui_font_km_160,
                                               LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj,
                        UI_COLOR_RPM_TEXT,
                        LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "0000");
                }

                // ------------------------
                // arc rpm 0–40 (0–4k)
                // ------------------------
                {
                    lv_obj_t *obj = lv_arc_create(parent_obj);
                    objects.arc_rpm_0_40 = obj;
                    lv_obj_set_pos(obj, UI_ARC_X, UI_ARC_Y);
                    lv_obj_set_size(obj, UI_ARC_SIZE, UI_ARC_SIZE);
                    lv_arc_set_range(obj, 0, 40);
                    lv_arc_set_value(obj, 0);
                    lv_arc_set_bg_start_angle(obj, 90);
                    lv_arc_set_bg_end_angle(obj, 270);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);

                    // knob background invisible
                    lv_obj_set_style_bg_opa(obj, 0, LV_PART_KNOB | LV_STATE_DEFAULT);

                    // background arc (track)
                    lv_obj_set_style_arc_rounded(obj, false, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_arc_width(obj, 65, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_arc_color(obj, lv_color_hex(0xff2f3237),
                                               LV_PART_MAIN | LV_STATE_DEFAULT);

                    // indicator arc (RPM) – make it GREEN
                    lv_obj_set_style_arc_rounded(obj, false,
                                                 LV_PART_INDICATOR | LV_STATE_DEFAULT);
                    lv_obj_set_style_arc_width(obj, 45,
                                               LV_PART_INDICATOR | LV_STATE_DEFAULT);
                    lv_obj_set_style_arc_color(obj, UI_RPM_ARC_COLOR,
                                               LV_PART_INDICATOR | LV_STATE_DEFAULT);
                }

                // ------------------------
                // bar rpm 40–80 (4–8k) with gradient
                // ------------------------
                {
                    lv_obj_t *obj = lv_bar_create(parent_obj);
                    objects.bar_rpm_40_80 = obj;
                    lv_obj_set_pos(obj, UI_BAR_X, UI_BAR_Y);
                    lv_obj_set_size(obj, UI_BAR_W, UI_BAR_H);

                    // 0..40 = 4000..8000 rpm (ecu bridge maps into this)
                    lv_bar_set_range(obj, 0, 40);
                    lv_bar_set_value(obj, 0, LV_ANIM_OFF);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);

                    // track
                    lv_obj_set_style_radius(obj, 0,
                                             LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(0xff2f3237),
                                              LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_opa(obj, 255,
                                            LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_all(obj, 2,
                                             LV_PART_MAIN | LV_STATE_DEFAULT);

                    // indicator – theme color → red gradient
                    lv_obj_set_style_radius(obj, 8,
                                             LV_PART_INDICATOR | LV_STATE_DEFAULT);

                    // start color from theme
                    lv_obj_set_style_bg_color(obj,
                        lv_color_hex(theme_colors[active_theme_index][0]),
                        LV_PART_INDICATOR | LV_STATE_DEFAULT);

                    // end color = red
                    lv_obj_set_style_bg_grad_color(obj, lv_color_hex(0xFF0000),
                                                   LV_PART_INDICATOR | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_HOR,
                                                 LV_PART_INDICATOR | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER,
                                             LV_PART_INDICATOR | LV_STATE_DEFAULT);
                }

                // ------------------------
                // scale numbers around arc (Hollow38 font + color)
                // ------------------------
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, UI_SCALE_0_X, UI_SCALE_0_Y);
                    add_style_label_rpm_value(obj);
                    lv_obj_set_style_text_font(obj, &ui_font_Hollow85,
                                               LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, UI_COLOR_SCALE_TEXT,
                                                LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "0");
                }
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, UI_SCALE_1_X, UI_SCALE_1_Y);
                    add_style_label_rpm_value(obj);
                    lv_obj_set_style_text_font(obj, &ui_font_Hollow85,
                                               LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, UI_COLOR_SCALE_TEXT,
                                                LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "1");
                }
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, UI_SCALE_2_X, UI_SCALE_2_Y);
                    add_style_label_rpm_value(obj);
                    lv_obj_set_style_text_font(obj, &ui_font_Hollow85,
                                               LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, UI_COLOR_SCALE_TEXT,
                                                LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "2");
                }
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, UI_SCALE_3_X, UI_SCALE_3_Y);
                    add_style_label_rpm_value(obj);
                    lv_obj_set_style_text_font(obj, &ui_font_Hollow85,
                                               LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, UI_COLOR_SCALE_TEXT,
                                                LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "3");
                }
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, UI_SCALE_4_X, UI_SCALE_4_Y);
                    add_style_label_rpm_value(obj);
                    lv_obj_set_style_text_font(obj, &ui_font_Hollow85,
                                               LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, UI_COLOR_SCALE_TEXT,
                                                LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "4");
                }
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, UI_SCALE_5_X, UI_SCALE_5_Y);
                    add_style_label_rpm_value(obj);
                    lv_obj_set_style_text_font(obj, &ui_font_Hollow85,
                                               LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, UI_COLOR_SCALE_TEXT,
                                                LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "5");
                }
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, UI_SCALE_6_X, UI_SCALE_6_Y);
                    add_style_label_rpm_value(obj);
                    lv_obj_set_style_text_font(obj, &ui_font_Hollow85,
                                               LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, UI_COLOR_SCALE_TEXT,
                                                LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "6");
                }
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, UI_SCALE_7_X, UI_SCALE_7_Y);
                    add_style_label_rpm_value(obj);
                    lv_obj_set_style_text_font(obj, &ui_font_Hollow85,
                                               LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, UI_COLOR_SCALE_TEXT,
                                                LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "7");
                }
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, UI_SCALE_8_X, UI_SCALE_8_Y);
                    add_style_label_rpm_value(obj);
                    lv_obj_set_style_text_font(obj, &ui_font_Hollow85,
                                               LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, UI_COLOR_SCALE_TEXT,
                                                LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "8");
                }
            }
        }

        //
        // ECU DATA BAR (Batt + Lambda) centered at bottom
        //
        {
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.container_throttle = obj;
            lv_obj_set_pos(obj, UI_INFO_X, UI_INFO_Y);
            lv_obj_set_size(obj, UI_INFO_W, UI_INFO_H);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

            {
                lv_obj_t *parent_obj = obj;

                // battery voltage – left side
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.label_batt = obj;
                    lv_obj_set_pos(obj, UI_BATT_LABEL_X, UI_BATT_LABEL_Y);
                    lv_obj_set_style_text_font(obj, &ui_font_Hollow85,   // CHANGED
                                               LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, UI_COLOR_INFO_TEXT,
                                                LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "Batt --.-V");
                }

                // lambda value – below batt
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.label_lambda = obj;
                    lv_obj_set_pos(obj, UI_LAMBDA_LABEL_X, UI_LAMBDA_LABEL_Y);
                    lv_obj_set_style_text_font(obj, &ui_font_Hollow85,   // CHANGED
                                               LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, UI_COLOR_INFO_TEXT,
                                                LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "Lambda ---");
                }
            }
        }

        //
        // Marker buttons bottom-left / bottom-right
        //
        {
            // Left marker button
            btn_marker_left = lv_btn_create(parent_obj);
            lv_obj_set_pos(btn_marker_left, UI_MARKER_LEFT_X, UI_MARKER_BTN_Y);
            lv_obj_set_size(btn_marker_left, UI_MARKER_BTN_W, UI_MARKER_BTN_H);
            marker_btn_update_style(btn_marker_left, marker_left_active);
            lv_obj_add_event_cb(btn_marker_left, marker_left_btn_event_cb, LV_EVENT_CLICKED, NULL);

            lv_obj_t *lblL = lv_label_create(btn_marker_left);
            lv_obj_set_style_text_font(lblL, &ui_font_Hollow38,
                                       LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(lblL, UI_COLOR_INFO_TEXT,
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(lblL, "Mode 1");
            lv_obj_center(lblL);

            // Right marker button
            btn_marker_right = lv_btn_create(parent_obj);
            lv_obj_set_pos(btn_marker_right, UI_MARKER_RIGHT_X, UI_MARKER_BTN_Y);
            lv_obj_set_size(btn_marker_right, UI_MARKER_BTN_W, UI_MARKER_BTN_H);
            marker_btn_update_style(btn_marker_right, marker_right_active);
            lv_obj_add_event_cb(btn_marker_right, marker_right_btn_event_cb, LV_EVENT_CLICKED, NULL);

            lv_obj_t *lblR = lv_label_create(btn_marker_right);
            lv_obj_set_style_text_font(lblR, &ui_font_Hollow38,
                                       LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(lblR, UI_COLOR_INFO_TEXT,
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(lblR, "Mode 2");
            lv_obj_center(lblR);
        }

        //
        // Temp label directly above Mode 2 button (no box)
        // (objects.label_throttle is reused as Temp label by ecu_ui_bridge)
        //
        {
            lv_obj_t *lbl_temp = lv_label_create(parent_obj);
            objects.label_throttle = lbl_temp;
            lv_obj_set_pos(lbl_temp, UI_THR_ABOVE_X - 80, UI_THR_ABOVE_Y);
            lv_obj_set_style_text_font(lbl_temp, &ui_font_Hollow38,  // CHANGED
                                       LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(lbl_temp, UI_COLOR_INFO_TEXT,
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(lbl_temp, "Temp --.-°C");
        }
    }

    tick_screen_main();
}

void tick_screen_main() {
}

void change_color_theme(uint32_t theme_index) {
    active_theme_index = theme_index;

    lv_obj_set_style_bg_color(objects.main,
        lv_color_hex(theme_colors[theme_index][1]),
        LV_PART_MAIN | LV_STATE_DEFAULT);

    // big RPM text keeps custom color
    lv_obj_set_style_text_color(objects.label_rpm,
        UI_COLOR_RPM_TEXT,
        LV_PART_MAIN | LV_STATE_DEFAULT);

    // keep bar color tied to theme (start of gradient)
    lv_obj_set_style_bg_color(objects.bar_rpm_40_80,
        lv_color_hex(theme_colors[theme_index][0]),
        LV_PART_INDICATOR | LV_STATE_DEFAULT);

    // RPM arc stays green even if theme changes
    lv_obj_set_style_arc_color(objects.arc_rpm_0_40,
        UI_RPM_ARC_COLOR,
        LV_PART_INDICATOR | LV_STATE_DEFAULT);

    // re-apply marker button styles (in case you later theme them)
    marker_btn_update_style(btn_marker_left,  marker_left_active);
    marker_btn_update_style(btn_marker_right, marker_right_active);

    lv_obj_invalidate(objects.main);
}

uint32_t theme_colors[1][2] = {
    { 0xffff2d2e, 0xff000000 },
};

typedef void (*tick_screen_func_t)();
tick_screen_func_t tick_screen_funcs[] = {
    tick_screen_main,
};

void tick_screen(int screen_index) {
    tick_screen_funcs[screen_index]();
}

void tick_screen_by_id(enum ScreensEnum screenId) {
    tick_screen_funcs[screenId - 1]();
}

void create_screens() {
    lv_disp_t *dispp = lv_disp_get_default();
    lv_theme_t *theme = lv_theme_default_init(dispp,
                                              lv_palette_main(LV_PALETTE_BLUE),
                                              lv_palette_main(LV_PALETTE_RED),
                                              true,
                                              LV_FONT_DEFAULT);
    lv_disp_set_theme(dispp, theme);

    create_screen_main();
}
