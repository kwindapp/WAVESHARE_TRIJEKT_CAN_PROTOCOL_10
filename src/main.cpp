#include <Arduino.h>
#include <esp_display_panel.hpp>
#include <lvgl.h>
#include "lvgl_v8_port.h"

#include "ui.h"
#include "ecu_data.h"              // <- struct ECUData + extern ECUData ecu
#include "waveshare_twai_port.h"
#include <stdbool.h>   // if not already present

bool TEST_MODE = false;    // or true, if you want test mode enabled by default

using namespace esp_panel::drivers;
using namespace esp_panel::board;

// =====================================================
// GLOBAL ECU INSTANCE (ONLY DEFINITION!)
// =====================================================
ECUData ecu;

// =====================================================
// CAN CONSTANTS & HELPERS
// =====================================================
const uint16_t MAX_RPM = 8000;
const uint32_t RPM_TIMEOUT_MS = 2000;   // FIX: prevent random RPM = 0

#define RPM_SMOOTHING      0.3f
#define TEMP_SMOOTHING     0.05f
#define VOLTAGE_SMOOTHING  1.1f
#define LAMBDA_SMOOTHING   0.03f

uint32_t lastRpmUpdateMs = 0;
uint16_t rpm_max = 0;

// Validation helpers
bool isValidRPM(uint16_t r)      { return (r > 0 && r <= MAX_RPM); }
bool isValidTemp(float t)        { return (t > -20 && t < 200); }
bool isValidBatt(float v)        { return (v > 8.0f && v < 16.5f); }
bool isValidLambda(float l)      { return (l >= 0.5f && l <= 3.0f); }

// BE helpers
static inline uint16_t U16BE(const uint8_t *p) {
    return (uint16_t(p[0]) << 8) | p[1];
}
static inline int16_t S16BE(const uint8_t *p) {
    return (int16_t)((uint16_t(p[0]) << 8) | p[1]);
}

// =====================================================
// CAN DECODER FOR ID 0x770
// =====================================================
void processECUData_770(const twai_message_t &msg) {
    uint32_t now = millis();
    const uint8_t *d = msg.data;

    // -------- Battery --------
    if (d[0]==0x00 && d[2]==0x00 && d[3]==0x00 &&
        d[6]==0x00 && d[7]==0x00 && (d[4]!=0x00 || d[5]!=0x00))
    {
        int16_t raw = S16BE(&d[4]);
        float newV = raw * 0.005f - 0.3f;

        if (isValidBatt(newV)) {
            if (ecu.batt == 0) ecu.batt = newV;
            else ecu.batt += VOLTAGE_SMOOTHING * (newV - ecu.batt);
            ecu.data_valid = true;
            ecu.last_update = now;
        }
        return;
    }

    // -------- Error frame --------
    if (d[0]==0x00 && d[1]==0x00 && d[5]==0x41 && d[6]==0x1E && d[7]==0xFC) {
        ecu.errors = d[4];
        ecu.data_valid = true;
        ecu.last_update = now;
        return;
    }

    // -------- Lambda --------
    if (d[0]==0xFB && d[1]==0xFA) {
        float l = d[2] / 100.0f;

        if (isValidLambda(l)) {
            if (ecu.lambda == 0) ecu.lambda = l;
            else ecu.lambda += LAMBDA_SMOOTHING * (l - ecu.lambda);

            ecu.lambda_valid = true;
            ecu.last_update = now;
            ecu.data_valid = true;
        }
        return;
    }

    // -------- RPM + TEMP --------
    if (d[4]==0x00 && d[5]==0x00 && d[6]==0x00 && d[7]==0x00 &&
        (d[2]!=0x00 || d[3]!=0x00))
    {
        uint16_t new_rpm = U16BE(&d[0]);
        int16_t rawT = S16BE(&d[2]);
        float newT = rawT * 0.01f - 100.0f;

        // FIX: direct RPM (no smoothing, responsive)
        if (isValidRPM(new_rpm)) {
            ecu.rpm = new_rpm;
            lastRpmUpdateMs = now;
        }

        if (isValidTemp(newT)) {
            if (ecu.motor == 0) ecu.motor = newT;
            else ecu.motor += TEMP_SMOOTHING * (newT - ecu.motor);
        }

        ecu.data_valid = true;
        ecu.last_update = now;
        return;
    }

    // -------- RPM only --------
    if (d[2]==0x00 && d[3]==0x00 && d[4]==0x00 && d[5]==0x00 &&
        d[6]==0x00 && d[7]==0x00)
    {
        uint16_t new_rpm = U16BE(&d[0]);

        if (isValidRPM(new_rpm)) {
            ecu.rpm = new_rpm;         // FIX: direct RPM
            ecu.data_valid = true;
            ecu.last_update = now;
            lastRpmUpdateMs = now;
        }
        return;
    }
}

// =====================================================
// Decode any CAN message
// =====================================================
void decodeCANMessage(const twai_message_t &m) {
    if (m.identifier == 0x770 && m.data_length_code >= 8) {
        processECUData_770(m);

        if (ecu.rpm > rpm_max) rpm_max = ecu.rpm;
    }
}

// =====================================================
// Housekeeping — FIXED (no 0 RPM drop)
// =====================================================
void updateECUHousekeeping() {
    uint32_t now = millis();
    // NO MORE ecu.rpm = 0 !!!
}

// =====================================================
// LVGL updates (mock functions but now real data)
// =====================================================
extern "C" void update_mock_rpm();
extern "C" void update_mock_throttle();    // Batt
extern "C" void update_mock_lambda();      // Lambda
extern "C" void update_mock_motor_temp();  // Temp
Board *board = nullptr;
bool can_ok = false;

// =====================================================
// SETUP
// =====================================================
void setup() {
    Serial.begin(115200);
    Serial.println("=== STARTING DASH ===");

    // 1) Create and init board object (but don't begin yet)
    Serial.println("[1] Initializing board object...");
    board = new Board();
    board->init();   // basic setup, but panel not fully started yet

    // 2) IMPORTANT: configure frame buffer count for RGB panel
    //    This MUST happen AFTER init() and BEFORE begin().
    //    Otherwise lvgl_port_init will request more frame buffers
    //    than the panel has, causing:
    //      esp_lcd_rgb_panel_get_frame_buffer(): invalid frame buffer number
#if LVGL_PORT_AVOID_TEARING_MODE
    {
        auto lcd = board->getLCD();

        // Set the number of frame buffers used by the RGB panel.
        // LVGL_PORT_DISP_BUFFER_NUM is defined in the esp_lvgl_port config.
        lcd->configFrameBufferNumber(LVGL_PORT_DISP_BUFFER_NUM);

    #if ESP_PANEL_DRIVERS_BUS_ENABLE_RGB && CONFIG_IDF_TARGET_ESP32S3
        auto lcd_bus = lcd->getBus();
        if (lcd_bus->getBasicAttributes().type == ESP_PANEL_BUS_TYPE_RGB) {
            // Bounce buffer to increase RGB bandwidth on ESP32-S3
            static_cast<BusRGB *>(lcd_bus)->configRGB_BounceBufferSize(
                lcd->getFrameWidth() * 10
            );
        }
    #endif
    }
#endif  // LVGL_PORT_AVOID_TEARING_MODE

    // 3) Now actually start the board (LCD, touch, backlight, etc.)
    Serial.println("[2] Calling board->begin()...");
    assert(board->begin());
    Serial.println("Board begin success.");

    // 4) Initialize LVGL with this LCD and touch
    Serial.println("[3] Initializing LVGL...");
    lvgl_port_init(board->getLCD(), board->getTouch());
    Serial.println("LVGL init OK.");

    // Load the generated UI from SquareLine
    lvgl_port_lock(-1);
    ui_init();
    lvgl_port_unlock();
    Serial.println("UI loaded.");

    // 5) Initialize CAN (TWAI)
    Serial.println("[4] Initializing CAN bus...");
    can_ok = waveshare_twai_init();
    if (can_ok)
        Serial.println("CAN OK");
    else
        Serial.println("CAN FAIL");

    // 6) Initialize ECU state
    ecu = ECUData{};
    lastRpmUpdateMs = millis();
    Serial.println("ECU state reset.");

    Serial.println("=== SETUP COMPLETE ===");
}
// =====================================================
// LOOP
// =====================================================
void loop() {
    // Read CAN
    if (can_ok) {
        twai_message_t m;
        if (twai_receive(&m, pdMS_TO_TICKS(10)) == ESP_OK) {
            decodeCANMessage(m);
        }
    }

    updateECUHousekeeping();

    // Update LVGL
    static uint32_t last_ui = 0;
    if (millis() - last_ui > 30) {
        lvgl_port_lock(-1);

        update_mock_rpm();          // RPM gauge
        update_mock_throttle();     // Batt
        update_mock_lambda();       // Lambda
        update_mock_motor_temp();   // Temp (above Mode 2)

        lvgl_port_unlock();

        last_ui = millis();
    }

    delay(1);
}