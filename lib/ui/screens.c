
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

int get_marker_right_state(void) {
    return marker_right_active;
}

extern bool TEST_MODE;

// -------------------------------------------------------------------
// Color configuration
// -------------------------------------------------------------------
#define UI_COLOR_TITLE      lv_color_hex(0xA4ADB3)
#define UI_COLOR_RPM_TEXT   lv_color_hex(0xD5DAE0)
#define UI_COLOR_SCALE_TEXT lv_color_hex(0xD1DDE3)
#define UI_COLOR_INFO_TEXT  lv_color_hex(0xA4ADB3)

// Enhanced gauge colors
#define UI_RPM_GREEN    lv_color_hex(0x00FF00)  // Green 0-2k
#define UI_RPM_YELLOW   lv_color_hex(0xFFFF00)  // Yellow 2-4k
#define UI_RPM_ORANGE   lv_color_hex(0xFFA500)  // Orange 4-6k
#define UI_RPM_RED      lv_color_hex(0xFF0000)  // Red 6-8k
#define UI_GAUGE_BG     lv_color_hex(0x2F3237)  // Dark track

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

// RPM big label
static const int UI_RPM_LABEL_X = 350;
static const int UI_RPM_LABEL_Y = 98;
static const int UI_RPM_LABEL_W = 491;

// RPM arc
static const int UI_ARC_X = 0;
static const int UI_ARC_Y = 0;
static const int UI_ARC_SIZE = 345;

// RPM bar
static const int UI_BAR_X = 380;
static const int UI_BAR_Y = 70;
static const int UI_BAR_W = 300;
static const int UI_BAR_H = 20;

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
static const int UI_SCALE_5_X = 239;
static const int UI_SCALE_5_Y = 15;
static const int UI_SCALE_6_X = 280;
static const int UI_SCALE_6_Y = 90;
static const int UI_SCALE_7_X = 280;
static const int UI_SCALE_7_Y = 180;
static const int UI_SCALE_8_X = 280;
static const int UI_SCALE_8_Y = 380;

// ECU info container
static const int UI_INFO_W = 450;
static const int UI_INFO_H = 165;
static const int UI_INFO_X = (UI_SCREEN_W - UI_INFO_W) / 2;
static const int UI_INFO_Y = 340;

// Batt / Lambda positions
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

// Temp label
static const int UI_THR_ABOVE_X  = UI_MARKER_RIGHT_X + 50;
static const int UI_THR_ABOVE_Y  = UI_MARKER_BTN_Y  +80;

// -------------------------------------------------------------------
// Marker button style helper
// -------------------------------------------------------------------
static lv_obj_t *btn_marker_left  = NULL;
static lv_obj_t *btn_marker_right = NULL;

static void marker_btn_update_style(lv_obj_t *btn, int active) {
    if (!btn) return;

    lv_color_t bg = active ? lv_color_hex(0x006400) : lv_color_hex(0x303030);

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
        marker_right_active = !marker_right_active;
        marker_btn_update_style(btn_marker_right, marker_right_active);
        TEST_MODE = (marker_right_active != 0);
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
            lv_obj_align(title1, LV_ALIGN_TOP_MID, 0, UI_TITLE_Y + 90);
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

                // RPM Arc - Range 0-40
                {
                    lv_obj_t *obj = lv_arc_create(parent_obj);
                    objects.arc_rpm_0_40 = obj;
                    lv_obj_set_pos(obj, UI_ARC_X, UI_ARC_Y);
                    lv_obj_set_size(obj, UI_ARC_SIZE, UI_ARC_SIZE);
                    lv_arc_set_range(obj, 0, 80);
                    lv_arc_set_value(obj, 0);
                    lv_arc_set_bg_start_angle(obj, 90);
                    lv_arc_set_bg_end_angle(obj, 380);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);

                    // Knob invisible
                    lv_obj_set_style_bg_opa(obj, 0, LV_PART_KNOB | LV_STATE_DEFAULT);

                    // Background arc
                    lv_obj_set_style_arc_rounded(obj, true, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_arc_width(obj, 70, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_arc_color(obj, lv_color_hex(0xFF2F3237),
                                               LV_PART_MAIN | LV_STATE_DEFAULT);

                    // Indicator arc
                    lv_obj_set_style_arc_rounded(obj, true,
                                                 LV_PART_INDICATOR | LV_STATE_DEFAULT);
                    lv_obj_set_style_arc_width(obj, 50,
                                               LV_PART_INDICATOR | LV_STATE_DEFAULT);
                    lv_obj_set_style_arc_color(obj, UI_RPM_GREEN,
                                               LV_PART_INDICATOR | LV_STATE_DEFAULT);
                }

                // RPM Bar - Range 0-80
                {
                    lv_obj_t *obj = lv_bar_create(parent_obj);
                    objects.bar_rpm_40_80 = obj;
                    lv_obj_set_pos(obj, UI_BAR_X, UI_BAR_Y);
                    lv_obj_set_size(obj, UI_BAR_W, UI_BAR_H);

                    lv_bar_set_range(obj, 0, 80);
                    lv_bar_set_value(obj, 0, LV_ANIM_OFF);
                    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);

                    // Track background
                    lv_obj_set_style_radius(obj, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_color(obj, lv_color_hex(0xFF2F3237),
                                              LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_bg_opa(obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_pad_all(obj, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_obj_set_style_border_color(obj, lv_color_hex(0x666666),
                                                  LV_PART_MAIN | LV_STATE_DEFAULT);

                    // Indicator
                    lv_obj_set_style_radius(obj, 18, LV_PART_INDICATOR | LV_STATE_DEFAULT);
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

            // DISABLE RIGHT BUTTON SCROLLING
            lv_obj_clear_flag(btn_marker_right, LV_OBJ_FLAG_SCROLLABLE);

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
