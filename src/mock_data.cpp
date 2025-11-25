/*
CAN Data Type   Stored in       Formula / Details
Battery Voltage ecu.batt        (S16BE(d+4) * 0.005) - 0.3
ECU Error Code  ecu.errors      d[4]
Lambda Value    ecu.lambda      d[2] / 100.0
Lambda Valid    ecu.lambda_valid Set to true on valid frame
RPM             ecu.rpm         U16BE(d+0)
Coolant Temp    ecu.motor       (S16BE(d+2) * 0.01) - 100
*/

#include "mock_data.h"
#include <Arduino.h>
#include <lvgl.h>
#include <math.h>
#include "ui.h"
#include "screens.h"
#include "ecu_data.h"

extern objects_t objects;

static const int MAX_RPM = 8000;

// =====================================================
// RPM UPDATE FROM REAL ECU DATA  (UNCHANGED)
// =====================================================
void update_mock_rpm() {
    static uint32_t last_update = 0;
    static int last_rpm_drawn = -1;

    uint32_t current_time = millis();

    // Update at most every 30 ms
    if (current_time - last_update < 30) {
        return;
    }
    last_update = current_time;

    int rpm_value = (int)ecu.rpm;
    if (rpm_value < 0) rpm_value = 0;
    if (rpm_value > MAX_RPM) rpm_value = MAX_RPM;

    if (rpm_value == last_rpm_drawn) return;
    last_rpm_drawn = rpm_value;

    // ---- Update label_rpm ----
    char rpm_text[8];
    snprintf(rpm_text, sizeof(rpm_text), "%04d", rpm_value);
    lv_label_set_text(objects.label_rpm, rpm_text);

    // ---- RPM arc + bar ----
    if (rpm_value <= 4000) {
        int arc_value = (rpm_value * 40) / 4000;
        lv_arc_set_value(objects.arc_rpm_0_40, arc_value);
        lv_bar_set_value(objects.bar_rpm_40_80, 0, LV_ANIM_OFF);
    } else {
        lv_arc_set_value(objects.arc_rpm_0_40, 40);
        int bar_value = ((rpm_value - 4000) * 40) / 4000;
        lv_bar_set_value(objects.bar_rpm_40_80, bar_value, LV_ANIM_OFF);
    }
}

// =====================================================
// BATTERY DISPLAY (Batt 13.87V) – label_batt
// =====================================================
void update_mock_throttle() {   // name kept for compatibility
    static uint32_t last_update = 0;
    uint32_t now = millis();

    if (now - last_update < 200)
        return;
    last_update = now;

    char text[32];
    // ecu.batt may be 0.0 at startup if no CAN yet
    snprintf(text, sizeof(text), "Batt %.2fV", ecu.batt);

    lv_label_set_text(objects.label_batt, text);
    // Debug to see it is called:
    // Serial.println(text);
}

// =====================================================
// LAMBDA DISPLAY (Lambda 1.02 / Lambda ---) – label_lambda
// =====================================================
void update_mock_lambda() {
    static uint32_t last_update = 0;
    uint32_t now = millis();

    if (now - last_update < 200)
        return;
    last_update = now;

    char text[32];

    if (ecu.lambda_valid) {
        snprintf(text, sizeof(text), "Lambda %.2f", ecu.lambda);
    } else {
        snprintf(text, sizeof(text), "Lambda ---");
    }

    lv_label_set_text(objects.label_lambda, text);
    // Serial.println(text);
}

// =====================================================
// MOTOR / COOLANT TEMP DISPLAY (Temp 89.3°C) – label_throttle
// =====================================================
void update_mock_motor_temp() {
    static uint32_t last_update = 0;
    uint32_t now = millis();

    if (now - last_update < 200)
        return;
    last_update = now;

    char text[32];
    snprintf(text, sizeof(text), "Temp %.1f°C", ecu.motor);

    lv_label_set_text(objects.label_throttle, text);
    // Serial.println(text);
}
