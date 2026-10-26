
/*
 * ============================================================
 * WAVESHARE ESP32-S3 / TRIJEKT PREMIUM 3.0
 * PORSCHE 911 SC 3.0 - MODE 1 DASHBOARD
 * ============================================================
 *
 * LIVE CAN DATA - NO SIMULATION
 *
 * ecu.rpm         = Engine RPM
 * ecu.batt        = Battery voltage
 * ecu.throttle_dk = Throttle position
 * ecu.motor       = Engine temperature
 *
 * EEZ WIDGETS:
 * label_rpm      = RPM
 * label_batt     = Battery
 * label_lambda   = Throttle
 * label_throttle = Engine Temperature
 *
 * LVGL 8.4
 */

#include "mock_data.h"

#include <Arduino.h>
#include <lvgl.h>
#include <math.h>
#include <stdio.h>

#include "ui.h"
#include "screens.h"
#include "ecu_data.h"

extern objects_t objects;

LV_FONT_DECLARE(ui_font_Hollow38);
LV_FONT_DECLARE(ui_font_Hollow85);

// ============================================================
// GENERAL SETTINGS
// ============================================================

static const int MAX_RPM = 8000;

static const uint32_t RPM_UPDATE_MS = 30;
static const uint32_t SENSOR_UPDATE_MS = 200;

// Live CAN getters from main.cpp
extern "C" float mode1_air_temp_c(void);
extern "C" float mode1_fuel_consumption_lph(void);

// ============================================================
// CUSTOM POSITIONS AND FONTS - MODE 1
// ============================================================
//
// Existing EEZ widgets:
// X/Y = offset from original EEZ Studio position.
//
// Air/Fuel:
// X/Y = offset from right edge of Battery/Throttle.
//
// Positive X = RIGHT
// Negative X = LEFT
// Positive Y = DOWN
// Negative Y = UP
//
// ============================================================

// BATTERY
static const int BATT_X = -40;
static const int BATT_Y = 60;
static const lv_font_t *BATT_FONT = &ui_font_Hollow38;

// THROTTLE
static const int THROTTLE_X = -40;
static const int THROTTLE_Y = 35;
static const lv_font_t *THROTTLE_FONT = &ui_font_Hollow38;

// ENGINE TEMPERATURE
static const int TEMP_X = -390;
static const int TEMP_Y = -160;
static const lv_font_t *TEMP_FONT = &ui_font_Hollow85;

// AIR TEMPERATURE - NEXT TO BATTERY
static const int AIR_X = 90;
static const int AIR_Y = 0;
static const lv_font_t *AIR_FONT = &ui_font_Hollow38;

// FUEL CONSUMPTION - NEXT TO THROTTLE
static const int FUEL_X = 80;
static const int FUEL_Y = 0;
static const lv_font_t *FUEL_FONT = &ui_font_Hollow38;

// ============================================================
// TEXT COLORS
// ============================================================

static const uint32_t BATT_COLOR = 0xA8AAB3;
static const uint32_t THROTTLE_COLOR = 0xA8AAB3;
static const uint32_t TEMP_COLOR = 0xA8AAB3;
static const uint32_t AIR_COLOR = 0xA8AAB3;
static const uint32_t FUEL_COLOR = 0xA8AAB3;

// ============================================================
// ENGINE OIL TEMPERATURE COLOR SETTINGS
// ============================================================
//
// These thresholds assume ecu.motor is OIL temperature.
//
// Below 80C    = LIGHT BLUE
// 80 - 109.9C  = GREEN
// 110 - 119.9C = ORANGE
// 120C+        = RED
//
// ============================================================

static const float TEMP_GREEN_THRESHOLD = 80.0f;
static const float TEMP_ORANGE_THRESHOLD = 110.0f;
static const float TEMP_RED_THRESHOLD = 120.0f;

static const char *TEMP_LIGHT_BLUE_HEX = "7DDFFF";
static const char *TEMP_GREEN_HEX = "00FF00";
static const char *TEMP_ORANGE_HEX = "FFAA00";
static const char *TEMP_RED_HEX = "FF0000";

// ============================================================
// NEW AIR / FUEL LABELS
// ============================================================

static lv_obj_t *airLabel = nullptr;
static lv_obj_t *fuelLabel = nullptr;

static bool extraLabelsInitialized = false;

static void init_extra_labels()
{
    if (extraLabelsInitialized ||
        !objects.label_batt ||
        !objects.label_lambda)
    {
        return;
    }

    lv_obj_t *screen =
        lv_obj_get_screen(objects.label_batt);

    if (!screen)
        return;

    airLabel = lv_label_create(screen);
    fuelLabel = lv_label_create(screen);

    if (!airLabel || !fuelLabel)
        return;

    // ========================================================
    // AIR TEMPERATURE
    // ========================================================

    lv_obj_set_style_text_font(
        airLabel,
        AIR_FONT,
        LV_PART_MAIN
    );

    lv_obj_set_style_text_color(
        airLabel,
        lv_color_hex(AIR_COLOR),
        LV_PART_MAIN
    );

    lv_obj_set_style_bg_opa(
        airLabel,
        LV_OPA_TRANSP,
        LV_PART_MAIN
    );

    lv_obj_set_style_pad_all(
        airLabel,
        0,
        LV_PART_MAIN
    );

    lv_label_set_text(
        airLabel,
        "Air ---C"
    );

    // ========================================================
    // FUEL CONSUMPTION
    // ========================================================

    lv_obj_set_style_text_font(
        fuelLabel,
        FUEL_FONT,
        LV_PART_MAIN
    );

    lv_obj_set_style_text_color(
        fuelLabel,
        lv_color_hex(FUEL_COLOR),
        LV_PART_MAIN
    );

    lv_obj_set_style_bg_opa(
        fuelLabel,
        LV_OPA_TRANSP,
        LV_PART_MAIN
    );

    lv_obj_set_style_pad_all(
        fuelLabel,
        0,
        LV_PART_MAIN
    );

    lv_label_set_text(
        fuelLabel,
        "Fuel ---L/h"
    );

    // ========================================================
    // CALCULATE SCREEN POSITIONS
    // ========================================================

    lv_obj_update_layout(screen);

    lv_area_t screenArea;
    lv_area_t battArea;
    lv_area_t throttleArea;

    lv_obj_get_coords(
        screen,
        &screenArea
    );

    lv_obj_get_coords(
        objects.label_batt,
        &battArea
    );

    lv_obj_get_coords(
        objects.label_lambda,
        &throttleArea
    );

    // Air Temp next to Battery
    lv_obj_set_pos(
        airLabel,
        battArea.x2 - screenArea.x1 + 1 + AIR_X,
        battArea.y1 - screenArea.y1 + AIR_Y
    );

    // Fuel next to Throttle
    lv_obj_set_pos(
        fuelLabel,
        throttleArea.x2 - screenArea.x1 + 1 + FUEL_X,
        throttleArea.y1 - screenArea.y1 + FUEL_Y
    );

    lv_obj_move_foreground(airLabel);
    lv_obj_move_foreground(fuelLabel);

    extraLabelsInitialized = true;

    Serial.println(
        "MODE 1: Air Temp and Fuel labels created"
    );
}

// ============================================================
// UPDATE AIR TEMP AND FUEL
// ============================================================

static void update_extra_labels()
{
    init_extra_labels();

    if (!extraLabelsInitialized)
        return;

    static uint32_t lastUpdate = 0;

    uint32_t now = millis();

    if (now - lastUpdate < SENSOR_UPDATE_MS)
        return;

    lastUpdate = now;

    char buf[40];

    // AIR TEMPERATURE
    snprintf(
        buf,
        sizeof(buf),
        "Air Temp %.1fC",
        (double)mode1_air_temp_c()
    );

    lv_label_set_text(
        airLabel,
        buf
    );

    // FUEL CONSUMPTION
    snprintf(
        buf,
        sizeof(buf),
        "Fuel %.1fL/h",
        (double)mode1_fuel_consumption_lph()
    );

    lv_label_set_text(
        fuelLabel,
        buf
    );
}

// ============================================================
// INTERNAL STATE
// ============================================================

static bool fonts_applied = false;
static bool positions_applied = false;

// ============================================================
// APPLY INDEPENDENT FONTS
// ============================================================

static void apply_sensor_fonts()
{
    if (fonts_applied)
        return;

    if (!objects.label_batt ||
        !objects.label_lambda ||
        !objects.label_throttle)
    {
        return;
    }

    // ========================================================
    // BATTERY
    // ========================================================

    lv_obj_set_style_text_font(
        objects.label_batt,
        BATT_FONT,
        LV_PART_MAIN
    );

    lv_obj_set_style_text_color(
        objects.label_batt,
        lv_color_hex(BATT_COLOR),
        LV_PART_MAIN
    );

    // ========================================================
    // THROTTLE
    // ========================================================

    lv_obj_set_style_text_font(
        objects.label_lambda,
        THROTTLE_FONT,
        LV_PART_MAIN
    );

    lv_obj_set_style_text_color(
        objects.label_lambda,
        lv_color_hex(THROTTLE_COLOR),
        LV_PART_MAIN
    );

    // ========================================================
    // ENGINE TEMPERATURE
    // ========================================================

    lv_obj_clear_flag(
        objects.label_throttle,
        LV_OBJ_FLAG_HIDDEN
    );

    lv_obj_set_style_text_font(
        objects.label_throttle,
        TEMP_FONT,
        LV_PART_MAIN
    );

    lv_obj_set_style_text_color(
        objects.label_throttle,
        lv_color_hex(TEMP_COLOR),
        LV_PART_MAIN
    );

    // Enable individual text recoloring.
    lv_label_set_recolor(
        objects.label_throttle,
        true
    );

    fonts_applied = true;

    Serial.println(
        "MODE 1: CUSTOM SENSOR FONTS APPLIED"
    );
}

// ============================================================
// APPLY INDEPENDENT POSITIONS
// ============================================================

static void apply_sensor_positions()
{
    if (positions_applied)
        return;

    if (!objects.label_batt ||
        !objects.label_lambda ||
        !objects.label_throttle)
    {
        return;
    }

    lv_obj_update_layout(
        lv_obj_get_screen(objects.label_batt)
    );

    // ========================================================
    // BATTERY
    // ========================================================

    lv_obj_t *battery = objects.label_batt;

    lv_coord_t batt_original_x =
        lv_obj_get_x(battery);

    lv_coord_t batt_original_y =
        lv_obj_get_y(battery);

    lv_obj_set_pos(
        battery,
        batt_original_x + BATT_X,
        batt_original_y + BATT_Y
    );

    // ========================================================
    // THROTTLE
    // ========================================================

    lv_obj_t *throttle = objects.label_lambda;

    lv_coord_t throttle_original_x =
        lv_obj_get_x(throttle);

    lv_coord_t throttle_original_y =
        lv_obj_get_y(throttle);

    lv_obj_set_pos(
        throttle,
        throttle_original_x + THROTTLE_X,
        throttle_original_y + THROTTLE_Y
    );

    // ========================================================
    // ENGINE TEMPERATURE
    // ========================================================

    lv_obj_t *temperature = objects.label_throttle;

    lv_coord_t temp_original_x =
        lv_obj_get_x(temperature);

    lv_coord_t temp_original_y =
        lv_obj_get_y(temperature);

    lv_obj_set_pos(
        temperature,
        temp_original_x + TEMP_X,
        temp_original_y + TEMP_Y
    );

    positions_applied = true;

    Serial.println();
    Serial.println("====================================");
    Serial.println("MODE 1 CUSTOM POSITIONS");
    Serial.println("====================================");

    Serial.printf(
        "BATTERY  X:%d Y:%d\n",
        (int)lv_obj_get_x(battery),
        (int)lv_obj_get_y(battery)
    );

    Serial.printf(
        "THROTTLE X:%d Y:%d\n",
        (int)lv_obj_get_x(throttle),
        (int)lv_obj_get_y(throttle)
    );

    Serial.printf(
        "TEMP     X:%d Y:%d\n",
        (int)lv_obj_get_x(temperature),
        (int)lv_obj_get_y(temperature)
    );

    Serial.println("====================================");
}

// ============================================================
// INITIALIZE SENSOR APPEARANCE
// ============================================================

static void init_sensor_appearance()
{
    apply_sensor_fonts();
    apply_sensor_positions();
    update_extra_labels();
}

// ============================================================
// RPM UPDATE - ORIGINAL LOGIC
// ============================================================

void update_mock_rpm()
{
    static uint32_t last_update = 0;
    static int last_rpm_value = -1;

    uint32_t current_time = millis();

    if (current_time - last_update < RPM_UPDATE_MS)
        return;

    last_update = current_time;

    if (!objects.label_rpm ||
        !objects.arc_rpm_0_40 ||
        !objects.bar_rpm_40_80)
    {
        return;
    }

    int rpm_value = (int)ecu.rpm;

    if (rpm_value < 0)
        rpm_value = 0;

    if (rpm_value > MAX_RPM)
        rpm_value = MAX_RPM;

    if (rpm_value == last_rpm_value)
        return;

    last_rpm_value = rpm_value;

    // ========================================================
    // RPM TEXT
    // ========================================================

    char rpm_text[8];

    snprintf(
        rpm_text,
        sizeof(rpm_text),
        "%04d",
        rpm_value
    );

    lv_label_set_text(
        objects.label_rpm,
        rpm_text
    );

    // ========================================================
    // RPM ARC
    // ========================================================

    int arc_value =
        (rpm_value * 80) / MAX_RPM;

    if (arc_value > 80)
        arc_value = 80;

    lv_arc_set_value(
        objects.arc_rpm_0_40,
        arc_value
    );

    // ========================================================
    // ARC COLOR
    // ========================================================

    if (rpm_value <= 2000)
    {
        lv_obj_set_style_arc_color(
            objects.arc_rpm_0_40,
            lv_color_hex(0x00FF00),
            LV_PART_INDICATOR | LV_STATE_DEFAULT
        );
    }
    else if (rpm_value <= 4000)
    {
        lv_obj_set_style_arc_color(
            objects.arc_rpm_0_40,
            lv_color_hex(0xFFFF00),
            LV_PART_INDICATOR | LV_STATE_DEFAULT
        );
    }
    else
    {
        lv_obj_set_style_arc_color(
            objects.arc_rpm_0_40,
            lv_color_hex(0xFFA500),
            LV_PART_INDICATOR | LV_STATE_DEFAULT
        );
    }

    // ========================================================
    // RPM BAR
    // ========================================================

    int bar_value =
        (rpm_value * 80) / MAX_RPM;

    lv_bar_set_value(
        objects.bar_rpm_40_80,
        bar_value,
        LV_ANIM_OFF
    );

    lv_color_t bar_color;

    if (rpm_value <= 2000)
    {
        bar_color = lv_color_hex(0x00FF00);
    }
    else if (rpm_value <= 4000)
    {
        bar_color = lv_color_hex(0xFFFF00);
    }
    else if (rpm_value <= 6000)
    {
        bar_color = lv_color_hex(0xFFA500);
    }
    else
    {
        bar_color = lv_color_hex(0xFF0000);
    }

    lv_obj_set_style_bg_color(
        objects.bar_rpm_40_80,
        bar_color,
        LV_PART_INDICATOR | LV_STATE_DEFAULT
    );

    lv_obj_set_style_bg_grad_color(
        objects.bar_rpm_40_80,
        bar_color,
        LV_PART_INDICATOR | LV_STATE_DEFAULT
    );

    lv_obj_set_style_bg_grad_dir(
        objects.bar_rpm_40_80,
        LV_GRAD_DIR_NONE,
        LV_PART_INDICATOR | LV_STATE_DEFAULT
    );

    // ========================================================
    // ORIGINAL DEBUG OUTPUT
    // ========================================================

    Serial.print("RPM: ");
    Serial.print(rpm_value);

    Serial.print(" Bar Value: ");
    Serial.print(bar_value);

    Serial.print(" Color: ");

    if (rpm_value <= 2000)
        Serial.println("GREEN");
    else if (rpm_value <= 4000)
        Serial.println("YELLOW");
    else if (rpm_value <= 6000)
        Serial.println("ORANGE");
    else
        Serial.println("RED");
}

// ============================================================
// BATTERY DISPLAY
// ============================================================

void update_mock_throttle()
{
    static uint32_t last_update = 0;

    init_sensor_appearance();

    uint32_t now = millis();

    if (now - last_update < SENSOR_UPDATE_MS)
        return;

    last_update = now;

    if (!objects.label_batt)
        return;

    char text[32];

    snprintf(
        text,
        sizeof(text),
        "Batt %.2fV",
        (double)ecu.batt
    );

    lv_label_set_text(
        objects.label_batt,
        text
    );
}

// ============================================================
// THROTTLE DISPLAY - ORIGINAL LAMBDA WIDGET
// ============================================================

void update_mock_lambda()
{
    static uint32_t last_update = 0;

    init_sensor_appearance();

    uint32_t now = millis();

    if (now - last_update < SENSOR_UPDATE_MS)
        return;

    last_update = now;

    if (!objects.label_lambda)
        return;

    char text[32];

    snprintf(
        text,
        sizeof(text),
        "Throttle %.1f%%",
        (double)ecu.throttle_dk
    );

    lv_label_set_text(
        objects.label_lambda,
        text
    );
}

// ============================================================
// ENGINE TEMPERATURE DISPLAY
// ============================================================
//
// Only the temperature VALUE changes color.
//
// Below 80C     = Light Blue
// 80 - 109.9C   = Green
// 110 - 119.9C  = Orange
// 120C+         = Red
//
// "Engine Temp" stays grey.
//
// ============================================================

void update_mock_motor_temp()
{
    static uint32_t last_update = 0;

    init_sensor_appearance();

    uint32_t now = millis();

    if (now - last_update < SENSOR_UPDATE_MS)
        return;

    last_update = now;

    if (!objects.label_throttle)
        return;

    float temp = ecu.motor;

    // ========================================================
    // SELECT TEMPERATURE COLOR
    // ========================================================

    const char *tempColor;

    if (temp < TEMP_GREEN_THRESHOLD)
    {
        // LIGHT BLUE
        tempColor = TEMP_LIGHT_BLUE_HEX;
    }
    else if (temp < TEMP_ORANGE_THRESHOLD)
    {
        // GREEN
        tempColor = TEMP_GREEN_HEX;
    }
    else if (temp < TEMP_RED_THRESHOLD)
    {
        // ORANGE
        tempColor = TEMP_ORANGE_HEX;
    }
    else
    {
        // RED
        tempColor = TEMP_RED_HEX;
    }

    // ========================================================
    // BUILD LABEL TEXT
    // ========================================================

    char text[80];

    snprintf(
        text,
        sizeof(text),
        "Engine Temp #%s %.1fC#",
        tempColor,
        (double)temp
    );

    // Enable recoloring only on Engine Temp.
    lv_label_set_recolor(
        objects.label_throttle,
        true
    );

    lv_label_set_text(
        objects.label_throttle,
        text
    );
}
