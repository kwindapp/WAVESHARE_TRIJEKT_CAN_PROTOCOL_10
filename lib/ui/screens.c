
#include <string.h>
#include <math.h>
#include <stdio.h>

#include "screens.h"
#include "images.h"
#include "fonts.h"
#include "actions.h"
#include "vars.h"
#include "styles.h"
#include "ui.h"

objects_t objects;
lv_obj_t *tick_value_change_obj;
uint32_t active_theme_index = 0;

// -------------------------------------------------------------------
// Internal button state
// -------------------------------------------------------------------
static int marker_left_active  = 0;
static int marker_right_active = 0;
static int marker_mode3_active = 0;
static int marker_mode4_active = 0;

int get_marker_right_state(void) {
    return marker_right_active;
}

extern bool TEST_MODE;

// -------------------------------------------------------------------
// Color configuration
// -------------------------------------------------------------------
#define UI_COLOR_TITLE      lv_color_hex(0x5E5A5A)
#define UI_COLOR_RPM_TEXT   lv_color_hex(0x5E5A5A)
#define UI_COLOR_SCALE_TEXT lv_color_hex(0xA8AABD)
#define UI_COLOR_INFO_TEXT  lv_color_hex(0xA4ADB3)

// Enhanced gauge colors
#define UI_RPM_GREEN    lv_color_hex(0x00FF00)  // Green 0-2k
#define UI_RPM_YELLOW   lv_color_hex(0xFFFF00)  // Yellow 2-4k
#define UI_RPM_ORANGE   lv_color_hex(0xFFA500)  // Orange 4-6k
#define UI_RPM_RED      lv_color_hex(0xFF0000)  // Red 6-8k
#define UI_GAUGE_BG     lv_color_hex(0x5E5A5A)  // Dark track

// -------------------------------------------------------------------
// Layout constants
// -------------------------------------------------------------------
static const int UI_SCREEN_W = 800;
static const int UI_SCREEN_H = 480;
static const int UI_TITLE_Y = 5;

// Gauge container
static const int UI_GAUGE_X = 10;
static const int UI_GAUGE_Y = 80;
static const int UI_GAUGE_W = 760;
static const int UI_GAUGE_H = 350;
// MODE 1
static const int UI_MODE1_BTN_X = 30;
static const int UI_MODE1_BTN_Y = 400;
// MODE 2
static const int UI_MODE2_BTN_X = 650;
static const int UI_MODE2_BTN_Y = 180;
// MODE 3 and MODE 4 (editable screen coordinates)
static const int UI_MODE3_BTN_X = 95;
static const int UI_MODE3_BTN_Y = 225;
static const int UI_MODE4_BTN_X = 650;
static const int UI_MODE4_BTN_Y = 260;
static const int UI_MODE_BTN_RADIUS = 20;
// RPM big label
static const int UI_RPM_LABEL_X = 230;
static const int UI_RPM_LABEL_Y = 98;
static const int UI_RPM_LABEL_W = 491;

// RPM arc
static const int UI_ARC_X = 0;
static const int UI_ARC_Y = 0;
static const int UI_ARC_SIZE = 345;

// RPM bar
static const int UI_BAR_X = 172;
static const int UI_BAR_Y = 0;
static const int UI_BAR_W = 565;
static const int UI_BAR_H = 70;

// Scale text positions
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
static const int UI_SCALE_5_X = 254;
static const int UI_SCALE_5_Y = -10;
static const int UI_SCALE_6_X = 378;
static const int UI_SCALE_6_Y = -10;
static const int UI_SCALE_7_X = 502;
static const int UI_SCALE_7_Y = -10;
static const int UI_SCALE_8_X = 626;
static const int UI_SCALE_8_Y = -10;

// ECU info container
static const int UI_INFO_W = 450;
static const int UI_INFO_H = 165;
static const int UI_INFO_X = (UI_SCREEN_W - UI_INFO_W) / 2;
static const int UI_INFO_Y = 340;

// Batt / Lambda positions
static const int UI_BATT_LABEL_X   = 90;
static const int UI_BATT_LABEL_Y   = -20;
static const int UI_LAMBDA_LABEL_X = 90;
static const int UI_LAMBDA_LABEL_Y = 40;

// Bottom marker buttons
static const int UI_MARKER_BTN_W = 120;
static const int UI_MARKER_BTN_H = 60;
static const int UI_MARKER_BTN_Y = UI_SCREEN_H - UI_MARKER_BTN_H - 20;
static const int UI_MARKER_LEFT_X  = 30;
static const int UI_MARKER_RIGHT_X = UI_SCREEN_W - UI_MARKER_BTN_W - 30;

// Temp label
static const int UI_THR_ABOVE_X  = UI_MARKER_RIGHT_X + 50;
static const int UI_THR_ABOVE_Y  = UI_MARKER_BTN_Y  +80;

// -------------------------------------------------------------------
// Marker button style helper
// -------------------------------------------------------------------
static lv_obj_t *btn_marker_left  = NULL;
static lv_obj_t *btn_marker_right = NULL;
static lv_obj_t *btn_marker_mode3 = NULL;
static lv_obj_t *btn_marker_mode4 = NULL;

static void marker_btn_update_style(lv_obj_t *btn, int active) {
    if (!btn) return;

    lv_color_t bg = active ? lv_color_hex(0x5D6E5D) : lv_color_hex(0x303030);

    lv_obj_set_style_bg_color(btn, bg, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(btn, UI_MODE_BTN_RADIUS, LV_PART_MAIN | LV_STATE_DEFAULT);
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
        marker_right_active = !marker_right_active;
        marker_btn_update_style(btn_marker_right, marker_right_active);
        TEST_MODE = (marker_right_active != 0);
    }
}

// Mode 3 / 4 are independent visual toggles. No ECU/Nixie action is assigned.
static void marker_mode3_btn_event_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        marker_mode3_active = !marker_mode3_active;
        marker_btn_update_style(btn_marker_mode3, marker_mode3_active);
    }
}

static void marker_mode4_btn_event_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        marker_mode4_active = !marker_mode4_active;
        marker_btn_update_style(btn_marker_mode4, marker_mode4_active);
    }
}

// -------------------------------------------------------------------
// CREATE MAIN SCREEN
// -------------------------------------------------------------------
void create_screen_main() {
    lv_obj_t *obj = lv_obj_create(0);
    objects.main = obj;

    // DISABLE MAIN SCREEN SCROLLING
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_pos(obj, 0, 0);
    lv_obj_set_size(obj, UI_SCREEN_W, UI_SCREEN_H);
    lv_obj_set_style_bg_color(obj, lv_color_hex(theme_colors[active_theme_index][1]),
                              LV_PART_MAIN | LV_STATE_DEFAULT);

    {
        lv_obj_t *parent_obj = obj;

        // Top title labels
        {
            lv_obj_t *title = lv_label_create(parent_obj);
            lv_label_set_text(title, "RWB JANINE DASH");
            lv_obj_set_style_text_font(title, &ui_font_Hollow85,
                                       LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(title, UI_COLOR_TITLE,
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_align(title, LV_ALIGN_TOP_MID, 0, UI_TITLE_Y);

            lv_obj_t *title1 = lv_label_create(parent_obj);
            lv_label_set_text(title1, "RAUH Welt BEGRIFF");
            lv_obj_set_style_text_font(title1, &ui_font_t20,
                                       LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(title1, UI_COLOR_TITLE,
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_align(title1, LV_ALIGN_TOP_MID, 0, UI_TITLE_Y + 155);
        }

        // Gauge container
        {
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.container_gauge = obj;

            // DISABLE GAUGE CONTAINER SCROLLING
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);

            lv_obj_set_pos(obj, UI_GAUGE_X, UI_GAUGE_Y);
            lv_obj_set_size(obj, UI_GAUGE_W, UI_GAUGE_H);
            lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

            {
                lv_obj_t *parent_obj = obj;

                // RPM label
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.label_rpm = obj;
                    lv_obj_set_pos(obj, UI_RPM_LABEL_X, UI_RPM_LABEL_Y);
                    lv_obj_set_size(obj, UI_RPM_LABEL_W, LV_SIZE_CONTENT);
                    lv_obj_set_style_text_font(obj, &ui_font_km_160,
                                               LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, UI_COLOR_RPM_TEXT,
                                                LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "0000");

                    lv_obj_move_foreground(obj);
                }

                // Dedicated grey track: separate LVGL arc below the live indicator.
                // Its indicator is hidden, so RPM updates cannot recolor this track.
                {
                    lv_obj_t *track = lv_arc_create(parent_obj);
                    lv_obj_set_pos(track, UI_ARC_X, UI_ARC_Y);
                    lv_obj_set_size(track, UI_ARC_SIZE, UI_ARC_SIZE);
                    lv_obj_clear_flag(track, LV_OBJ_FLAG_CLICKABLE);
                    lv_arc_set_mode(track, LV_ARC_MODE_NORMAL);
                    lv_arc_set_range(track, 0, 4000);
                    lv_arc_set_bg_angles(track, 70, 270);
                    lv_arc_set_value(track, 0);
                    lv_obj_set_style_bg_opa(track, LV_OPA_TRANSP, LV_PART_MAIN);
                    lv_obj_set_style_bg_opa(track, LV_OPA_TRANSP, LV_PART_KNOB);
                    lv_obj_set_style_arc_width(track, 70, LV_PART_MAIN);
                    lv_obj_set_style_arc_rounded(track, false, LV_PART_MAIN);
                    lv_obj_set_style_arc_color(track, lv_color_hex(0x777D84), LV_PART_MAIN);
                    lv_obj_set_style_arc_opa(track, LV_OPA_COVER, LV_PART_MAIN);

                    lv_obj_set_style_arc_opa(track, LV_OPA_TRANSP, LV_PART_INDICATOR);
                    // Keep track above the transparent container background.
                    // It is created before the live arc, so the indicator remains on top.
                }

                // Live RPM indicator: no background, so grey track stays visible.
                {
                    lv_obj_t *arc = lv_arc_create(parent_obj);
                    objects.arc_rpm_0_40 = arc;
                    lv_obj_set_pos(arc, UI_ARC_X, UI_ARC_Y);
                    lv_obj_set_size(arc, UI_ARC_SIZE, UI_ARC_SIZE);
                    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
                    lv_arc_set_mode(arc, LV_ARC_MODE_NORMAL);
                    lv_arc_set_range(arc, 0, 4000);
                    lv_arc_set_bg_angles(arc, 70, 270);
                    lv_arc_set_value(arc, 0);
                    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_MAIN);
                    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);
                    lv_obj_set_style_arc_opa(arc, LV_OPA_TRANSP, LV_PART_MAIN);
                    lv_obj_set_style_arc_width(arc, 50, LV_PART_INDICATOR);
                    lv_obj_set_style_arc_color(arc, UI_RPM_GREEN, LV_PART_INDICATOR);
                    lv_obj_set_style_arc_opa(arc, LV_OPA_COVER, LV_PART_INDICATOR);
                    lv_obj_set_style_arc_rounded(arc, false, LV_PART_INDICATOR);
                }

                // RPM Bar - Range 40-80 (4000-8000 RPM)
                {
                    lv_obj_t *obj = lv_bar_create(parent_obj);
                    objects.bar_rpm_40_80 = obj;
                    lv_obj_set_pos(obj, UI_BAR_X, UI_BAR_Y);
                    lv_obj_set_size(obj, UI_BAR_W, UI_BAR_H);

                    lv_bar_set_range(obj, 0, 4000);
                    lv_bar_set_value(obj, 0, LV_ANIM_OFF);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);

                    // Track background
                    lv_obj_set_style_radius(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(0x777D84),
                                              LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_all(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_color(obj, lv_color_hex(0x666666),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);

                    // Indicator
                    lv_obj_set_style_radius(obj, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, UI_RPM_GREEN,
                                              LV_PART_INDICATOR | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_NONE,
                                                 LV_PART_INDICATOR | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_opa(obj, 255, LV_PART_INDICATOR | LV_STATE_DEFAULT);
                }

                // Scale numbers
                struct {
                    int x, y;
                    char *text;
                } scale_points[] = {
                    {UI_SCALE_0_X, UI_SCALE_0_Y, "0"},
                    {UI_SCALE_1_X, UI_SCALE_1_Y, "1"},
                    {UI_SCALE_2_X, UI_SCALE_2_Y, "2"},
                    {UI_SCALE_3_X, UI_SCALE_3_Y, "3"},
                    {UI_SCALE_4_X, UI_SCALE_4_Y, "4"},
                    {UI_SCALE_5_X, UI_SCALE_5_Y, "5"},
                    {UI_SCALE_6_X, UI_SCALE_6_Y, "6"},
                    {UI_SCALE_7_X, UI_SCALE_7_Y, "7"},
                    {UI_SCALE_8_X, UI_SCALE_8_Y, "8"}
                };

                for(int i = 0; i < 9; i++) {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, scale_points[i].x, scale_points[i].y);
                    lv_obj_set_style_text_font(obj, &ui_font_Hollow85,
                                               LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, UI_COLOR_SCALE_TEXT,
                                                LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, scale_points[i].text);
                }
            }
        }

        // ECU Info Container
        {
            lv_obj_t *obj = lv_obj_create(parent_obj);
            objects.container_throttle = obj;

            // DISABLE ECU INFO CONTAINER SCROLLING
            lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);

            lv_obj_set_pos(obj, UI_INFO_X, UI_INFO_Y);
            lv_obj_set_size(obj, UI_INFO_W, UI_INFO_H);
            lv_obj_set_style_bg_opa(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

            {
                lv_obj_t *parent_obj = obj;

                // Battery voltage
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.label_batt = obj;
                    lv_obj_set_pos(obj, UI_BATT_LABEL_X, UI_BATT_LABEL_Y);
                    lv_obj_set_style_text_font(obj, &ui_font_Hollow85,
                                               LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, UI_COLOR_INFO_TEXT,
                                                LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "Batt --.-V");
                }

                // Lambda value
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    objects.label_lambda = obj;
                    lv_obj_set_pos(obj, UI_LAMBDA_LABEL_X, UI_LAMBDA_LABEL_Y);
                    lv_obj_set_style_text_font(obj, &ui_font_Hollow85,
                                               LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_text_color(obj, UI_COLOR_INFO_TEXT,
                                                LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text(obj, "Lambda ---");
                }
            }
        }

        // Marker buttons
        {
            // Left marker button
            btn_marker_left = lv_btn_create(parent_obj);

            // DISABLE LEFT BUTTON SCROLLING
            lv_obj_clear_flag(btn_marker_left, LV_OBJ_FLAG_SCROLLABLE);

            lv_obj_set_pos(btn_marker_left, UI_MODE1_BTN_X, UI_MODE1_BTN_Y);
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

            // DISABLE RIGHT BUTTON SCROLLING
            lv_obj_clear_flag(btn_marker_right, LV_OBJ_FLAG_SCROLLABLE);

            lv_obj_set_pos(btn_marker_right, UI_MODE2_BTN_X, UI_MODE2_BTN_Y);
            lv_obj_set_size(btn_marker_right, UI_MARKER_BTN_W, UI_MARKER_BTN_H);
            marker_btn_update_style(btn_marker_right, marker_right_active);
            lv_obj_add_event_cb(btn_marker_right, marker_right_btn_event_cb, LV_EVENT_CLICKED, NULL);

            lv_obj_t *lblR = lv_label_create(btn_marker_right);
            lv_obj_set_style_text_font(lblR, &ui_font_Hollow38,
                                       LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(lblR, UI_COLOR_INFO_TEXT,
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(lblR, "Mode 3");
            lv_obj_center(lblR);

            // Mode 3 button
            btn_marker_mode3 = lv_btn_create(parent_obj);
            lv_obj_clear_flag(btn_marker_mode3, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_pos(btn_marker_mode3, UI_MODE3_BTN_X , UI_MODE3_BTN_Y );
            lv_obj_set_size(btn_marker_mode3, UI_MARKER_BTN_W, UI_MARKER_BTN_H);
            marker_btn_update_style(btn_marker_mode3, marker_mode3_active);
            lv_obj_add_event_cb(btn_marker_mode3, marker_mode3_btn_event_cb, LV_EVENT_CLICKED, NULL);
            lv_obj_t *lbl3 = lv_label_create(btn_marker_mode3);
            lv_obj_set_style_text_font(lbl3, &ui_font_Hollow38, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(lbl3, UI_COLOR_INFO_TEXT, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(lbl3, "Mode 2");
            lv_obj_center(lbl3);

            // Mode 4 button
            btn_marker_mode4 = lv_btn_create(parent_obj);
            lv_obj_clear_flag(btn_marker_mode4, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_pos(btn_marker_mode4, UI_MODE4_BTN_X, UI_MODE4_BTN_Y);
            lv_obj_set_size(btn_marker_mode4, UI_MARKER_BTN_W, UI_MARKER_BTN_H);
            marker_btn_update_style(btn_marker_mode4, marker_mode4_active);
            lv_obj_add_event_cb(btn_marker_mode4, marker_mode4_btn_event_cb, LV_EVENT_CLICKED, NULL);
            lv_obj_t *lbl4 = lv_label_create(btn_marker_mode4);
            lv_obj_set_style_text_font(lbl4, &ui_font_Hollow38, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(lbl4, UI_COLOR_INFO_TEXT, LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(lbl4, "Mode 4");
            lv_obj_center(lbl4);
        }

        // Temperature label
        {
            lv_obj_t *lbl_temp = lv_label_create(parent_obj);
            objects.label_throttle = lbl_temp;
            lv_obj_set_pos(lbl_temp, UI_THR_ABOVE_X - 80, UI_THR_ABOVE_Y);
            lv_obj_set_style_text_font(lbl_temp, &ui_font_Hollow38,
                                       LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(lbl_temp, UI_COLOR_INFO_TEXT,
                                        LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_label_set_text(lbl_temp, "Temp --.-°C");
        }
    }

    tick_screen_main();
}

// -------------------------------------------------------------------
// SCREEN TICK
// -------------------------------------------------------------------
void tick_screen_main() {
    // This function is called periodically
}

// -------------------------------------------------------------------
// CHANGE COLOR THEME
// -------------------------------------------------------------------
void change_color_theme(uint32_t theme_index) {
    active_theme_index = theme_index;

    lv_obj_set_style_bg_color(objects.main,
        lv_color_hex(theme_colors[theme_index][1]),
        LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_text_color(objects.label_rpm,
        UI_COLOR_RPM_TEXT,
        LV_PART_MAIN | LV_STATE_DEFAULT);

    marker_btn_update_style(btn_marker_left, marker_left_active);
    marker_btn_update_style(btn_marker_right, marker_right_active);
    marker_btn_update_style(btn_marker_mode3, marker_mode3_active);
    marker_btn_update_style(btn_marker_mode4, marker_mode4_active);

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
