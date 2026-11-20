// RWB JANINE / AIR WERK - V8 (complete, compiles)
// Screens: MODE 1 (dashboard) / MODE 2 (Nixie) / MODE 3 (all ECU values)

#include <Arduino.h>
#include <esp_display_panel.hpp>
#include <lvgl.h>
#include "lvgl_v8_port.h"

#include "ui.h"
#include "screens.h"
#include "ecu_data.h"
#include "waveshare_twai_port.h"
#include "screenecu.h"

#include <driver/twai.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include <string.h>
#include <math.h>
#include <Preferences.h>
#include <WiFiManager.h>

using namespace esp_panel::drivers;
using namespace esp_panel::board;

LV_FONT_DECLARE(ui_font_Hollow38);

extern "C" {
    bool TEST_MODE = false;
}

// ============================================================
// FORWARD DECLARATIONS  (do not delete)
// ============================================================

extern "C" void update_mock_rpm();
extern "C" void update_mock_throttle();
extern "C" void update_mock_lambda();
extern "C" void update_mock_motor_temp();

extern "C" void rwb_set_test_mode_label(bool enabled);
extern "C" void open_trijekt_mode1(void);
extern "C" void open_trijekt_mode2(void);
extern "C" void open_trijekt_mode3(void);
extern "C" void rwb_toggle_test_mode(void);

static void createCANScreen();
static void updateCANScreen();
static void airCreateScreen();
static void airInit();
static void airTick();
static void airWifiTick();

static void sendEspNowValues();
static void printAllValues();

// ============================================================
// SETTINGS
// ============================================================

static constexpr uint32_t CAN_A = 700;
static constexpr uint32_t CAN_B = 710;

static constexpr uint32_t RPM_TIMEOUT_MS    = 2000;
static constexpr uint32_t SENSOR_TIMEOUT_MS = 5000;

static constexpr uint32_t UI_INTERVAL_MS        = 30;
static constexpr uint32_t ESPNOW_INTERVAL_MS    = 50;
static constexpr uint32_t DEBUG_INTERVAL_MS     = 1000;
static constexpr uint32_t CAN_DEBUG_INTERVAL_MS = 5000;

static constexpr uint16_t MAX_RPM = 8000;

static bool DASH_DEMO_MODE = false;

static constexpr int      CAN_BATCH_SIZE     = 96;
static constexpr uint32_t CAN_TIME_BUDGET_US = 2000;

static constexpr bool PRINT_LIVE_VALUES     = false;
static constexpr bool PRINT_CAN_DIAGNOSTICS = false;
static constexpr bool PRINT_LAMBDA_RAW      = false;
static constexpr int  CAN_VALUE_COUNT       = 20;

// ============================================================
// GLOBAL STATE
// ============================================================

ECUData ecu;
Board  *board  = nullptr;
bool    can_ok = false;

uint32_t lastRpmUpdateMs = 0;
uint32_t lastCanUpdateMs = 0;
uint16_t rpm_max = 0;

struct TrijektValues {
    uint16_t rpm         = 0;
    uint16_t torque      = 0;
    uint16_t motorStatus = 0;

    float fuelConsumption = 0.0f;

    float throttleDK    = 0.0f;
    float throttlePedal = 0.0f;
    uint16_t egasControl = 0;
    float egasTarget     = 0.0f;

    float engineTemp   = 0.0f;
    float airTemp      = 0.0f;
    float internalTemp = 0.0f;
    float oilTemp      = 0.0f;

    uint16_t intakePressure   = 0;
    uint16_t externalPressure = 0;
    uint16_t fuelPressure     = 0;
    uint16_t oilPressure      = 0;

    float lambda1 = 0.0f;
    float lambda2 = 0.0f;
    float battery = 0.0f;

    bool rpmValid      = false;
    bool throttleValid = false;
    bool tempValid     = false;
    bool oilTempValid  = false;
    bool pressureValid = false;
    bool lambda1Valid  = false;
    bool lambda2Valid  = false;
    bool batteryValid  = false;

    uint32_t lastRpmMs      = 0;
    uint32_t lastThrottleMs = 0;
    uint32_t lastTempMs     = 0;
    uint32_t lastOilMs      = 0;
    uint32_t lastPressureMs = 0;
    uint32_t lastLambdaMs   = 0;
    uint32_t lastBatteryMs  = 0;
};

TrijektValues trijekt;

extern "C" float mode1_air_temp_c(void)           { return trijekt.airTemp; }
extern "C" float mode1_fuel_consumption_lph(void) { return trijekt.fuelConsumption; }

// ============================================================
// DECODE HELPERS / VALIDITY
// ============================================================

static inline uint16_t U16LE(const uint8_t *d) {
    return uint16_t(d[0]) | (uint16_t(d[1]) << 8);
}
static inline int16_t S16LE(const uint8_t *d) { return (int16_t)U16LE(d); }

static bool validRPM(uint16_t v)  { return v <= MAX_RPM; }
static bool validTemp(float v)    { return v >= -40.0f && v <= 180.0f; }
static bool validBattery(float v) { return v >= 8.0f   && v <= 18.0f;  }
static bool validLambda(float v)  { return isfinite(v) && v >= 0.50f && v <= 10.00f; }

static bool fresh(bool valid, uint32_t lastMs,
                  uint32_t timeoutMs = SENSOR_TIMEOUT_MS) {
    return valid && (millis() - lastMs <= timeoutMs);
}

// ============================================================
// MODE 3 VALUE IDs / LAYOUT
// ============================================================

enum Mode2ValueID {
    M2_RPM = 0, M2_TORQUE, M2_STATUS, M2_FUEL,
    M2_THROTTLE, M2_PEDAL, M2_EGAS, M2_ETARGET,
    M2_ENGINE_TEMP, M2_AIR_TEMP, M2_ECU_TEMP, M2_OIL_TEMP,
    M2_INTAKE, M2_EXTERNAL, M2_FUEL_PRESSURE, M2_OIL_PRESSURE,
    M2_LAMBDA1, M2_LAMBDA2, M2_BATTERY, M2_MAX_RPM
};

struct Mode2Layout {
    bool enabled;
    int16_t x, y;
    const char *name;
};

static Mode2Layout mode2Layout[CAN_VALUE_COUNT] = {
    { true,   20,  65, "RPM"         },
    { true,   20, 100, "Torque"      },
    { true,   20, 135, "Status"      },
    { true,   20, 170, "Fuel"        },
    { true,   20, 205, "Throttle"    },
    { true,   20, 240, "Pedal"       },
    { true,   20, 275, "E-Gas"       },
    { true,   20, 310, "E-Target"    },
    { true,   20, 345, "Engine Temp" },
    { false,  20, 380, "Air Temp"    },

    { true,  410,  65, "ECU Temp"    },
    { true,  410, 100, "Oil Temp"    },
    { true,  410, 135, "Intake"      },
    { true,  410, 170, "External"    },
    { true,  410, 205, "Fuel Press"  },
    { true,  410, 240, "Oil Press"   },
    { true,  410, 275, "Lambda 1"    },
    { true,  410, 310, "Lambda 2"    },
    { true,  410, 345, "Battery"     },
    { true,  410, 380, "Max RPM"     }
};

// ============================================================
// CAN DIAGNOSTICS
// ============================================================

struct CANIDStats {
    uint32_t count = 0;
    uint32_t lastMs = 0;
    uint8_t lastData[8] = {0};
    uint8_t dlc = 0;
};

static CANIDStats canStats[2048];
static uint32_t totalCANFrames  = 0;
static uint32_t lastCANCount    = 0;
static uint32_t extendedFrames  = 0;
static uint32_t remoteFrames    = 0;
static uint32_t canLastReportMs = 0;

static void recordCANFrame(const twai_message_t &m) {
    totalCANFrames++;
    if (m.extd) { extendedFrames++; return; }
    if (m.rtr)  { remoteFrames++;   return; }
    if (m.identifier >= 2048) return;

    CANIDStats &s = canStats[m.identifier];
    s.count++;
    s.lastMs = millis();
    s.dlc = m.data_length_code;
    memset(s.lastData, 0, sizeof(s.lastData));
    uint8_t len = m.data_length_code;
    if (len > 8) len = 8;
    memcpy(s.lastData, m.data, len);
}

static void printCANDiagnostics() {
    uint32_t now     = millis();
    uint32_t elapsed = now - canLastReportMs;
    uint32_t newFrames = totalCANFrames - lastCANCount;
    float rate = (elapsed > 0) ? newFrames * 1000.0f / elapsed : 0.0f;

    Serial.println();
    Serial.println("====================================");
    Serial.println("CAN DIAGNOSTICS");
    Serial.println("====================================");
    Serial.printf("Total frames: %lu\n", (unsigned long)totalCANFrames);
    Serial.printf("Frame rate: %.1f/s\n", rate);
    Serial.printf("Extended: %lu\n", (unsigned long)extendedFrames);
    Serial.printf("Remote: %lu\n",   (unsigned long)remoteFrames);
    Serial.println("ID  COUNT  AGE(ms)  DATA");
    for (int id = 0; id < 2048; id++) {
        const CANIDStats &s = canStats[id];
        if (s.count == 0) continue;
        Serial.printf("%d  %lu  %lu  ",
            id, (unsigned long)s.count, (unsigned long)(now - s.lastMs));
        for (int i = 0; i < s.dlc && i < 8; i++)
            Serial.printf("%02X ", s.lastData[i]);
        Serial.println();
    }
    twai_status_info_t status{};
    if (twai_get_status_info(&status) == ESP_OK) {
        Serial.printf("RX queue: %lu\n",   (unsigned long)status.msgs_to_rx);
        Serial.printf("RX missed: %lu\n",  (unsigned long)status.rx_missed_count);
        Serial.printf("RX overrun: %lu\n", (unsigned long)status.rx_overrun_count);
        Serial.printf("Bus errors: %lu\n", (unsigned long)status.bus_error_count);
        Serial.printf("TX failed: %lu\n",  (unsigned long)status.tx_failed_count);
    }
    Serial.println("====================================");
    lastCANCount    = totalCANFrames;
    canLastReportMs = millis();
}

// ============================================================
// ESP-NOW
// ============================================================

static uint8_t espnow_broadcast_addr[] = { 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF };
static bool espnow_ready   = false;
static bool espnow_enabled = true;
static uint32_t lastEspNowSendMs = 0;
static const uint8_t ESPNOW_WIFI_CHANNEL = 1;

struct DashPacket {
    uint16_t rpm;
    float batt;
    float motor;
    float dk;
    float gp;
    uint8_t funk;
} __attribute__((packed));

void setEspNowEnabled(bool enabled) { espnow_enabled = enabled; }

static bool espnow_init_simple() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    esp_err_t channelErr = esp_wifi_set_channel(
        ESPNOW_WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
    if (channelErr != ESP_OK) {
        Serial.printf("ESP-NOW CHANNEL ERROR: %d\n", (int)channelErr);
        return false;
    }

    esp_err_t err = esp_now_init();
    if (err != ESP_OK && err != ESP_ERR_ESPNOW_EXIST) {
        Serial.println("ESP-NOW INIT FAILED");
        return false;
    }

    esp_now_peer_info_t peer{};
    memcpy(peer.peer_addr, espnow_broadcast_addr, 6);
    peer.channel = ESPNOW_WIFI_CHANNEL;
    peer.encrypt = false;

    if (!esp_now_is_peer_exist(espnow_broadcast_addr)) {
        err = esp_now_add_peer(&peer);
        if (err != ESP_OK) {
            Serial.printf("ESP-NOW PEER ERROR: %d\n", (int)err);
            return false;
        }
    }
    espnow_ready = true;
    Serial.println("ESP-NOW READY");
    Serial.println(WiFi.macAddress());
    return true;
}

static void sendEspNowValues() {
    if (!espnow_ready || !espnow_enabled) return;

    uint32_t now = millis();
    if (now - lastEspNowSendMs < ESPNOW_INTERVAL_MS) return;

    DashPacket pkt{};
    pkt.rpm   = ecu.data_valid ? ecu.rpm : 0;
    pkt.batt  = fresh(trijekt.batteryValid,  trijekt.lastBatteryMs)  ? ecu.batt           : 0.0f;
    pkt.motor = fresh(trijekt.tempValid,     trijekt.lastTempMs)     ? ecu.motor          : 0.0f;
    pkt.dk    = fresh(trijekt.throttleValid, trijekt.lastThrottleMs) ? ecu.throttle_dk    : 0.0f;
    pkt.gp    = fresh(trijekt.throttleValid, trijekt.lastThrottleMs) ? ecu.throttle_pedal : 0.0f;
    pkt.funk  = 0;

    esp_err_t err = esp_now_send(
        espnow_broadcast_addr,
        reinterpret_cast<const uint8_t*>(&pkt),
        sizeof(pkt));
    if (err != ESP_OK) {
        static uint32_t lastErrorMs = 0;
        if (now - lastErrorMs >= 5000) {
            Serial.printf("ESP-NOW SEND ERROR: %d\n", (int)err);
            lastErrorMs = now;
        }
    }
    lastEspNowSendMs = now;
}

// ============================================================
// CAN DECODER
// ============================================================

void decodeCANMessage(const twai_message_t &m) {
    if (m.extd || m.rtr) return;
    if (m.data_length_code != 8) return;

    uint32_t now = millis();
    const uint8_t *d = m.data;
    bool decoded = false;

    switch (m.identifier) {
    case CAN_A + 0: {
        uint16_t rpm = U16LE(d);
        if (!validRPM(rpm)) break;
        trijekt.rpm             = rpm;
        trijekt.torque          = U16LE(d + 2);
        trijekt.motorStatus     = U16LE(d + 4);
        trijekt.fuelConsumption = U16LE(d + 6) * 0.1f;
        trijekt.rpmValid        = true;
        trijekt.lastRpmMs       = now;
        ecu.rpm = rpm;
        lastRpmUpdateMs = now;
        if (rpm > rpm_max) rpm_max = rpm;
        decoded = true; break;
    }
    case CAN_A + 1: {
        trijekt.throttleDK    = U16LE(d)     * 0.1f;
        trijekt.throttlePedal = U16LE(d + 2) * 0.1f;
        trijekt.egasControl   = U16LE(d + 4);
        trijekt.egasTarget    = U16LE(d + 6) * 0.1f;
        trijekt.throttleValid = true;
        trijekt.lastThrottleMs = now;
        ecu.throttle_dk    = trijekt.throttleDK;
        ecu.throttle_pedal = trijekt.throttlePedal;
        decoded = true; break;
    }
    case CAN_A + 2: {
        float engineTemp   = S16LE(d)     * 0.1f;
        float airTemp      = S16LE(d + 2) * 0.1f;
        float internalTemp = S16LE(d + 6) * 0.1f;
        if (!validTemp(engineTemp)) break;
        trijekt.engineTemp   = engineTemp;
        trijekt.airTemp      = airTemp;
        trijekt.internalTemp = internalTemp;
        trijekt.tempValid    = true;
        trijekt.lastTempMs   = now;
        ecu.motor = engineTemp;
        decoded = true; break;
    }
    case CAN_A + 3: {
        float oilTemp = S16LE(d) * 0.1f;
        if (!validTemp(oilTemp)) break;
        trijekt.oilTemp      = oilTemp;
        trijekt.oilTempValid = true;
        trijekt.lastOilMs    = now;
        decoded = true; break;
    }
    case CAN_A + 4: {
        trijekt.intakePressure   = U16LE(d);
        trijekt.externalPressure = U16LE(d + 2);
        trijekt.fuelPressure     = U16LE(d + 4);
        trijekt.oilPressure      = U16LE(d + 6);
        trijekt.pressureValid    = true;
        trijekt.lastPressureMs   = now;
        decoded = true; break;
    }
    case CAN_A + 5: {
        float lambda1 = U16LE(d)     * 0.001f;
        float lambda2 = U16LE(d + 2) * 0.001f;

        static uint32_t lastLambdaDebugMs = 0;
        if (PRINT_LAMBDA_RAW && now - lastLambdaDebugMs >= 1000) {
            Serial.printf(
                "LAMBDA CAN ID=%lu RAW=%02X %02X %02X %02X %02X %02X %02X %02X | L1=%.3f L2=%.3f\n",
                (unsigned long)m.identifier,
                d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7],
                (double)lambda1, (double)lambda2);
            lastLambdaDebugMs = now;
        }

        trijekt.lambda1 = lambda1;
        trijekt.lambda2 = lambda2;
        trijekt.lambda1Valid = validLambda(lambda1);
        trijekt.lambda2Valid = validLambda(lambda2);
        trijekt.lastLambdaMs = now;
        if (trijekt.lambda1Valid) ecu.lambda = lambda1;
        decoded = true; break;
    }
    case CAN_B + 1: {
        float battery = U16LE(d + 2) * 0.001f;
        if (!validBattery(battery)) break;
        trijekt.battery       = battery;
        trijekt.batteryValid  = true;
        trijekt.lastBatteryMs = now;
        ecu.batt = battery;
        decoded = true; break;
    }
    default: break;
    }

    if (decoded) {
        lastCanUpdateMs = now;
        ecu.last_update = now;
    }
}

// ============================================================
// HOUSEKEEPING + DEMO
// ============================================================

void updateECUHousekeeping() {
    bool rpmFresh    = fresh(trijekt.rpmValid,     trijekt.lastRpmMs,    RPM_TIMEOUT_MS);
    bool lambdaFresh = fresh(trijekt.lambda1Valid, trijekt.lastLambdaMs);

    ecu.data_valid     = rpmFresh ? 1 : 0;
    ecu.lambda_valid   = lambdaFresh ? 1 : 0;
    ecu.engine_running = rpmFresh && ecu.rpm > 100;

    if (!rpmFresh) ecu.rpm = 0;
}

static void updateDashboardDemo() {
    if (!DASH_DEMO_MODE) return;

    const uint32_t now   = millis();
    const uint32_t phase = now % 16000UL;
    const uint32_t ramp  = (phase <= 8000UL) ? phase : 16000UL - phase;
    const uint16_t rpm   = (uint16_t)ramp;
    const float pct      = (float)rpm / 8000.0f;

    const float batt     = 12.0f + 2.4f * pct;
    const float engine   = 55.0f + 70.0f * pct;
    const float throttle = 100.0f * pct;

    ecu.rpm = rpm;
    ecu.batt = batt;
    ecu.motor = engine;
    ecu.throttle_dk = throttle;
    ecu.throttle_pedal = throttle;
    ecu.lambda = 0.85f + 0.25f * (1.0f - pct);
    ecu.data_valid = 1;
    ecu.lambda_valid = 1;
    ecu.engine_running = rpm > 100;
    ecu.last_update = now;

    trijekt.rpm = rpm;
    trijekt.rpmValid = true;
    trijekt.lastRpmMs = now;
    trijekt.battery = batt;
    trijekt.batteryValid = true;
    trijekt.lastBatteryMs = now;
    trijekt.engineTemp = engine;
    trijekt.tempValid = true;
    trijekt.lastTempMs = now;
    trijekt.throttleDK = throttle;
    trijekt.throttlePedal = throttle;
    trijekt.throttleValid = true;
    trijekt.lastThrottleMs = now;
    trijekt.fuelConsumption = 0.5f + 28.0f * pct;
    trijekt.torque = (uint16_t)(280.0f * pct);
    trijekt.motorStatus = rpm > 100 ? 1 : 0;
    trijekt.lambda1 = ecu.lambda;
    trijekt.lambda2 = ecu.lambda + 0.02f;
    trijekt.lambda1Valid = true;
    trijekt.lambda2Valid = true;
    trijekt.lastLambdaMs = now;
    trijekt.oilTemp = 45.0f + 65.0f * pct;
    trijekt.oilTempValid = true;
    trijekt.lastOilMs = now;
    trijekt.internalTemp = 25.0f + 25.0f * pct;
    trijekt.airTemp = 18.0f + 12.0f * pct;
    trijekt.intakePressure   = (uint16_t)(950.0f + 50.0f  * pct);
    trijekt.externalPressure = 1013;
    trijekt.fuelPressure     = (uint16_t)(300.0f + 50.0f  * pct);
    trijekt.oilPressure      = (uint16_t)(100.0f + 350.0f * pct);
    trijekt.pressureValid = true;
    trijekt.lastPressureMs = now;
    trijekt.egasControl = (uint16_t)throttle;
    trijekt.egasTarget  = throttle;
    if (rpm > rpm_max) rpm_max = rpm;
}

// ============================================================
// SERIAL LIVE VALUES
// ============================================================

static void printFloatValue(
    const char *label, float value, const char *unit,
    bool valid, uint32_t lastMs, uint8_t decimals = 1,
    uint32_t timeoutMs = SENSOR_TIMEOUT_MS)
{
    Serial.printf("%-19s : ", label);
    if (fresh(valid, lastMs, timeoutMs)) {
        Serial.print(value, decimals);
        if (unit && unit[0]) { Serial.print(" "); Serial.print(unit); }
        Serial.println();
    } else {
        Serial.println("--");
    }
}

static void printIntValue(
    const char *label, uint32_t value, const char *unit,
    bool valid, uint32_t lastMs,
    uint32_t timeoutMs = SENSOR_TIMEOUT_MS)
{
    Serial.printf("%-19s : ", label);
    if (fresh(valid, lastMs, timeoutMs)) {
        Serial.print(value);
        if (unit && unit[0]) { Serial.print(" "); Serial.print(unit); }
        Serial.println();
    } else {
        Serial.println("--");
    }
}

static void printAllValues() {
    Serial.println();
    Serial.println("====================================");
    Serial.println("TRIJEKT LIVE VALUES");
    Serial.println("====================================");

    printIntValue  ("RPM",          trijekt.rpm,             "rpm", trijekt.rpmValid,      trijekt.lastRpmMs,      RPM_TIMEOUT_MS);
    printIntValue  ("Torque",       trijekt.torque,          "Nm",  trijekt.rpmValid,      trijekt.lastRpmMs,      RPM_TIMEOUT_MS);
    printIntValue  ("Motor status", trijekt.motorStatus,     "",    trijekt.rpmValid,      trijekt.lastRpmMs,      RPM_TIMEOUT_MS);
    printFloatValue("Fuel",         trijekt.fuelConsumption, "L/h", trijekt.rpmValid,      trijekt.lastRpmMs, 1,   RPM_TIMEOUT_MS);
    printFloatValue("Throttle DK",  trijekt.throttleDK,      "%",   trijekt.throttleValid, trijekt.lastThrottleMs);
    printFloatValue("Pedal",        trijekt.throttlePedal,   "%",   trijekt.throttleValid, trijekt.lastThrottleMs);
    printIntValue  ("E-Gas",        trijekt.egasControl,     "",    trijekt.throttleValid, trijekt.lastThrottleMs);
    printFloatValue("E-Target",     trijekt.egasTarget,      "deg", trijekt.throttleValid, trijekt.lastThrottleMs);
    printFloatValue("Engine Temp",  trijekt.engineTemp,      "C",   trijekt.tempValid,     trijekt.lastTempMs);
    printFloatValue("Air Temp",     trijekt.airTemp,         "C",   trijekt.tempValid,     trijekt.lastTempMs);
    printFloatValue("ECU Temp",     trijekt.internalTemp,    "C",   trijekt.tempValid,     trijekt.lastTempMs);
    printFloatValue("Oil Temp",     trijekt.oilTemp,         "C",   trijekt.oilTempValid,  trijekt.lastOilMs);
    printIntValue  ("Intake",       trijekt.intakePressure,  "hPa", trijekt.pressureValid, trijekt.lastPressureMs);
    printIntValue  ("External",     trijekt.externalPressure,"hPa", trijekt.pressureValid, trijekt.lastPressureMs);
    printIntValue  ("Fuel Press",   trijekt.fuelPressure,    "kPa", trijekt.pressureValid, trijekt.lastPressureMs);
    printIntValue  ("Oil Press",    trijekt.oilPressure,     "kPa", trijekt.pressureValid, trijekt.lastPressureMs);
    printFloatValue("Lambda 1",     trijekt.lambda1,         "",    trijekt.lambda1Valid,  trijekt.lastLambdaMs, 3);
    printFloatValue("Lambda 2",     trijekt.lambda2,         "",    trijekt.lambda2Valid,  trijekt.lastLambdaMs, 3);
    printFloatValue("Battery",      trijekt.battery,         "V",   trijekt.batteryValid,  trijekt.lastBatteryMs, 2);

    Serial.printf("Max RPM: %u\n", rpm_max);
    Serial.printf("CAN: %s\n",     can_ok ? "READY" : "ERROR");
    Serial.printf("ESP-NOW: %s\n", espnow_ready ? "READY" : "ERROR");
    Serial.println("====================================");
}

// ============================================================
// SCREEN HANDLES
// ============================================================

static lv_obj_t *originalScreen = nullptr;
static lv_obj_t *canScreen      = nullptr;
static lv_obj_t *ecuTopBack     = nullptr; // unused; legacy compatibility
static lv_obj_t *airScreen      = nullptr;

static lv_obj_t *canValueLabels[CAN_VALUE_COUNT] = {};
static uint32_t lastCANScreenUpdate = 0;

// ============================================================
// NAVIGATION
// ============================================================

// One permanent LVGL screen: objects.main. All menus are child overlays.
// Touch events queue a request; the main LVGL loop applies it under the lock.
static lv_obj_t *settingsPanel = nullptr;
static volatile int requestedPage = -1;
static int currentPage = 0;

extern "C" void open_trijekt_mode1(void) { requestedPage = 0; }
extern "C" void open_trijekt_mode2(void) { requestedPage = 1; }
extern "C" void open_trijekt_mode3(void) { requestedPage = 2; }
extern "C" void open_trijekt_settings(void) { requestedPage = 3; }

extern "C" void dashboard_resume_animation(void);

static void createSettingsPanel();

// All navigation is applied in loop() while holding the LVGL mutex.
// Secondary pages are allocated only when first visited.
static void navigateToPage(int page) {
    if (!objects.main || page < 0 || page > 3) return;
    if (page == currentPage) return;

    Serial.printf("NAV REQUEST page=%d free_heap=%u free_psram=%u\n",
                  page, (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram());
    if (page == 1 && !airScreen) airCreateScreen();
    if (page == 2 && !canScreen) createCANScreen();
    if (page == 3 && !settingsPanel) createSettingsPanel();

    lv_obj_t *panel = page == 1 ? airScreen : page == 2 ? canScreen :
                      page == 3 ? settingsPanel : nullptr;
    if (page != 0 && !panel) {
        Serial.printf("NAV ERROR: page %d allocation failed\n", page);
        return;
    }
    if (airScreen) lv_obj_add_flag(airScreen, LV_OBJ_FLAG_HIDDEN);
    if (canScreen) lv_obj_add_flag(canScreen, LV_OBJ_FLAG_HIDDEN);
    if (settingsPanel) lv_obj_add_flag(settingsPanel, LV_OBJ_FLAG_HIDDEN);
    const int previousPage = currentPage;
    currentPage = page;
    if (panel) {
        lv_obj_clear_flag(panel, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(panel);
    }
    if (page == 0 && previousPage != 0) {
        dashboard_resume_animation();
        Serial.println("DASHBOARD ANIMATION RESTARTED");
    }
    lv_obj_invalidate(objects.main);
    Serial.printf("NAV PAGE=%d active=%p panel=%p\n",
                  page, (void *)lv_scr_act(), (void *)panel);
}

static void processNavigationRequest() {
    int next = requestedPage;
    if (next < 0) return;
    requestedPage = -1;
    navigateToPage(next);
}

// Keep Settings lightweight: plain clickable rows rather than nested
// buttons + labels + styles. This saves LVGL allocations and avoids
// the lv_label_create(btn) failure seen in the crash backtrace.
static lv_obj_t *settingsButton(lv_obj_t *parent, const char *caption,
                                int x, int y, int w, int h,
                                lv_event_cb_t cb) {
    if (!parent) return nullptr;
    lv_obj_t *row = lv_label_create(parent);
    if (!row) {
        Serial.printf("SETTINGS: label allocation failed: %s\n", caption);
        return nullptr;
    }
    lv_label_set_text(row, caption);
    lv_obj_set_pos(row, x, y);
    lv_obj_set_size(row, w, h);
    lv_obj_set_style_text_color(row, lv_color_hex(0xFFB466), 0);
    lv_obj_set_style_bg_color(row, lv_color_hex(0x242A32), 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(row, 10, 0);
    lv_obj_set_style_pad_left(row, 16, 0);
    lv_obj_set_style_pad_top(row, 18, 0);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    if (cb) lv_obj_add_event_cb(row, cb, LV_EVENT_PRESSED, nullptr);
    return row;
}

static void createSettingsPanel() {
    if (settingsPanel || !objects.main) return;
    Serial.printf("SETTINGS CREATE begin heap=%u psram=%u\n",
                  (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram());
    lv_obj_t *panel = lv_obj_create(objects.main);
    if (!panel) {
        Serial.println("SETTINGS ERROR: panel allocation failed");
        return;
    }
    settingsPanel = panel;
    lv_obj_set_size(panel, 800, 480);
    lv_obj_set_pos(panel, 0, 0);
    lv_obj_set_style_pad_all(panel, 0, 0);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x080B10), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);

    settingsButton(panel, "AIR WERK  |  SETTINGS", 25, 24, 480, 45, nullptr);
    settingsButton(panel, "< DASHBOARD", 595, 12, 190, 54,
        [](lv_event_t *e) { if (lv_event_get_code(e)==LV_EVENT_PRESSED) open_trijekt_mode1(); });
    settingsButton(panel, "BACKGROUND 1", 35, 100, 340, 65,
        [](lv_event_t *e) { if (lv_event_get_code(e)==LV_EVENT_PRESSED) change_color_theme(0); });
    settingsButton(panel, "BACKGROUND 2", 425, 100, 340, 65,
        [](lv_event_t *e) { if (lv_event_get_code(e)==LV_EVENT_PRESSED) change_color_theme(1); });
    settingsButton(panel, "BACKGROUND 3", 35, 175, 340, 65,
        [](lv_event_t *e) { if (lv_event_get_code(e)==LV_EVENT_PRESSED) change_color_theme(2); });
    settingsButton(panel, "BACKGROUND 4", 425, 175, 340, 65,
        [](lv_event_t *e) { if (lv_event_get_code(e)==LV_EVENT_PRESSED) change_color_theme(3); });
    settingsButton(panel, "NIXIE SETTINGS", 35, 255, 340, 70,
        [](lv_event_t *e) { if (lv_event_get_code(e)==LV_EVENT_PRESSED) open_trijekt_mode2(); });
    settingsButton(panel, "ECU DATA", 425, 255, 340, 70,
        [](lv_event_t *e) { if (lv_event_get_code(e)==LV_EVENT_PRESSED) open_trijekt_mode3(); });
    Serial.printf("SETTINGS CREATE done heap=%u psram=%u\n",
                  (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram());
}

// ============================================================
// TEST MODE TOGGLE
// ============================================================

extern "C" void rwb_toggle_test_mode(void) {
    DASH_DEMO_MODE = !DASH_DEMO_MODE;
    TEST_MODE = DASH_DEMO_MODE;

    if (!DASH_DEMO_MODE) {
        ecu = ECUData{};
        trijekt = TrijektValues{};
        lastRpmUpdateMs = 0;
        lastCanUpdateMs = 0;
        rpm_max = 0;
    }
    rwb_set_test_mode_label(DASH_DEMO_MODE);
    Serial.printf("DASH MODE: %s\n",
                  DASH_DEMO_MODE ? "TEST" : "REAL CAN");
}

// ============================================================
// MODE 3 SCREEN
// ============================================================

static void createCANScreen() {
    canScreen = screenecu_create(objects.main);
    for (int i = 0; i < CAN_VALUE_COUNT; ++i)
        canValueLabels[i] = screenecu_value_label(i);
    Serial.println("NEW MODULAR ECU SCREEN READY");
}

static void updateCANScreen() {
    if (!canScreen) return;
    if (currentPage != 2) return;

    uint32_t now = millis();
    if (now - lastCANScreenUpdate < 200) return;
    lastCANScreenUpdate = now;

    bool rpmOK      = fresh(trijekt.rpmValid,      trijekt.lastRpmMs,      RPM_TIMEOUT_MS);
    bool throttleOK = fresh(trijekt.throttleValid, trijekt.lastThrottleMs);
    bool tempOK     = fresh(trijekt.tempValid,     trijekt.lastTempMs);
    bool oilOK      = fresh(trijekt.oilTempValid,  trijekt.lastOilMs);
    bool pressureOK = fresh(trijekt.pressureValid, trijekt.lastPressureMs);
    bool lambda1OK  = fresh(trijekt.lambda1Valid,  trijekt.lastLambdaMs);
    bool lambda2OK  = fresh(trijekt.lambda2Valid,  trijekt.lastLambdaMs);
    bool batteryOK  = fresh(trijekt.batteryValid,  trijekt.lastBatteryMs);

    for (int i = 0; i < CAN_VALUE_COUNT; i++) {
        if (!mode2Layout[i].enabled) continue;
        lv_obj_t *label = canValueLabels[i];
        if (!label) continue;

        bool valid = false;
        float fv = 0.0f;
        uint32_t iv = 0;
        bool isFloat = false;
        int decimals = 1;
        const char *unit = "";

        switch (i) {
        case M2_RPM:           valid = rpmOK;      iv = trijekt.rpm;             break;
        case M2_TORQUE:        valid = rpmOK;      iv = trijekt.torque;          unit = " Nm"; break;
        case M2_STATUS:        valid = rpmOK;      iv = trijekt.motorStatus;     break;
        case M2_FUEL:          valid = rpmOK;      fv = trijekt.fuelConsumption; isFloat = true; unit = " L/h"; break;
        case M2_THROTTLE:      valid = throttleOK; fv = trijekt.throttleDK;      isFloat = true; unit = "%";   break;
        case M2_PEDAL:         valid = throttleOK; fv = trijekt.throttlePedal;   isFloat = true; unit = "%";   break;
        case M2_EGAS:          valid = throttleOK; iv = trijekt.egasControl;     break;
        case M2_ETARGET:       valid = throttleOK; fv = trijekt.egasTarget;      isFloat = true; unit = " deg"; break;
        case M2_ENGINE_TEMP:   valid = tempOK;     fv = trijekt.engineTemp;      isFloat = true; unit = " C";   break;
        case M2_AIR_TEMP:      valid = tempOK;     fv = trijekt.airTemp;         isFloat = true; unit = " C";   break;
        case M2_ECU_TEMP:      valid = tempOK;     fv = trijekt.internalTemp;    isFloat = true; unit = " C";   break;
        case M2_OIL_TEMP:      valid = oilOK;      fv = trijekt.oilTemp;         isFloat = true; unit = " C";   break;
        case M2_INTAKE:        valid = pressureOK; iv = trijekt.intakePressure;  unit = " hPa"; break;
        case M2_EXTERNAL:      valid = pressureOK; iv = trijekt.externalPressure;unit = " hPa"; break;
        case M2_FUEL_PRESSURE: valid = pressureOK; iv = trijekt.fuelPressure;    unit = " kPa"; break;
        case M2_OIL_PRESSURE:  valid = pressureOK; iv = trijekt.oilPressure;     unit = " kPa"; break;
        case M2_LAMBDA1:       valid = lambda1OK;  fv = trijekt.lambda1;         isFloat = true; decimals = 3; break;
        case M2_LAMBDA2:       valid = lambda2OK;  fv = trijekt.lambda2;         isFloat = true; decimals = 3; break;
        case M2_BATTERY:       valid = batteryOK;  fv = trijekt.battery;         isFloat = true; decimals = 2; unit = " V"; break;
        case M2_MAX_RPM:       valid = rpm_max > 0; iv = rpm_max;                break;
        default: break;
        }

        char text[80];
        if (!valid) {
            snprintf(text, sizeof(text), "%s: ---", mode2Layout[i].name);
        } else if (isFloat) {
            snprintf(text, sizeof(text), "%s: %.*f%s",
                     mode2Layout[i].name, decimals, (double)fv, unit);
        } else {
            snprintf(text, sizeof(text), "%s: %lu%s",
                     mode2Layout[i].name, (unsigned long)iv, unit);
        }
        lv_label_set_text(label, text);
    }
}

// ============================================================
// FIND MODE 2 BUTTON IN GENERATED UI
// ============================================================

// ============================================================
// AIR WERK (MODE 2 NIXIE)
// ============================================================

#ifndef AIRWERK_NIXIE_TX
#define AIRWERK_NIXIE_TX 44
#endif
#ifndef AIRWERK_NIXIE_RX
#define AIRWERK_NIXIE_RX 43
#endif

static HardwareSerial airSerial(2);
static constexpr bool AIRWERK_UART_ENABLED = true;
static Preferences airPrefs;
static lv_obj_t *airStatus = nullptr, *airList = nullptr;
static bool airLed = false, airBuzz = true, airAuto = false, airFade = false;
static uint8_t airColor = 0, airLedBright = 25, airTubeBright = 100;
static uint8_t airECUMode = 0, airAnimStyle = 0;
static bool airAnimating = false, airClock = true;
static bool airStopwatch = false, airSWRun = false;
static uint32_t airAnimStart = 0, airLastFrame = 0, airLastCycle = 0;
static uint32_t airLastLive = 0, airLastStop = 0;
static uint32_t airSWBase = 0, airSWStart = 0, airLastFade = 0;
static int airFadeVal = 0, airFadeDir = 1, airLastPulse = -1;
static char airDigits[7] = "------";
static const char *airColors[] = {"AMBER","RED","GREEN","BLUE","WHITE","PURPLE"};
static WiFiManager airWiFiManager;
static bool airWifiPortal = false;
static bool airWifiConnected = false;

static void airWifiStart();
static void airWifiStop();

static const char *airActions[] = {
    "CINEMATIC SHOW (COMBINED)", "RPM LEFT (TUBE 1)",
    "AUTO CINEMATIC 5 MIN", "RTC CLOCK", "SHOW 911930",
    "LED ON / OFF", "LED COLOR",
    "LED BRIGHTNESS +10", "LED FADE DEMO",
    "NIXIE BRIGHTNESS +10", "NIXIE BRIGHTNESS -10",
    "STARTUP REPLAY", "BUZZER ON / OFF", "BUZZER TEST",
    "MEGA STATUS", "STOPWATCH MODE", "STOPWATCH START / PAUSE",
    "STOPWATCH RESET", "BUZZER TONE",
    "NIXIE RPM", "NIXIE BATTERY", "NIXIE ENGINE TEMP",
    "NIXIE THROTTLE", "NIXIE PEDAL",
    "ECU DISPLAY OFF", "LED OFF", "MODE 3 - ALL ECU VALUES",
    "WIFI SETUP - START", "WIFI SETUP - STOP"
};

static void airCmd(const String &s) {
    if (AIRWERK_UART_ENABLED) {
        airSerial.println(s);
        if (s == "MODE:CLOCK") airSerial.println("DOTS:ON");
    } else {
        Serial.printf("AIR WERK RS485 DISABLED: %s\n", s.c_str());
    }
}

static void airDisplay(const char *digits) {
    if (strcmp(airDigits, digits) != 0) {
        strncpy(airDigits, digits, 6);
        airDigits[6] = 0;
        airCmd(String("DISPLAY:") + digits);
    }
}

static void airSave() {
    airPrefs.putBool("led", airLed);
    airPrefs.putBool("buzzer", airBuzz);
    airPrefs.putBool("autocine", airAuto);
    airPrefs.putUChar("color", airColor);
    airPrefs.putUChar("ledbright", airLedBright);
    airPrefs.putUChar("tube", airTubeBright);
}

static void airStopSW();

static void airSetClock() {
    airStopSW();
    airAnimating = false;
    airStopwatch = false;
    airECUMode = 0;
    airFade = false;
    airClock = true;
    airCmd("MODE:CLOCK");
    strcpy(airDigits, "------");
    airLastCycle = millis();
}

static void airStartAnimation(uint8_t style) {
    airAnimating = true;
    airAnimStyle = style;
    airClock = false;
    airStopwatch = false;
    airECUMode = 0;
    airFade = false;
    airLed = false;
    airAnimStart = millis();
    airLastFrame = 0;
    airLastPulse = -1;
    airCmd("LED:OFF");
    airCmd(String("NIXIE:BRIGHT:") + airTubeBright);
    strcpy(airDigits, "------");
}

static uint32_t airSWMillis() {
    return airSWBase + (airSWRun ? millis() - airSWStart : 0);
}

static void airStopSW() {
    if (airSWRun) airSWBase += millis() - airSWStart;
    airSWRun = false;
}

static void airUpdateStatus() {
    if (!airStatus || currentPage != 1) return;
    const char *mode = airAnimating ? "CINEMATIC"
                     : airECUMode   ? "LIVE ECU"
                     : airStopwatch ? "STOPWATCH"
                     : airClock     ? "CLOCK"
                                    : "MANUAL";
    lv_label_set_text_fmt(airStatus,
        "%s | %s | TUBE %u%% | LED %s %u%% | AUTO %s",
        mode, airDigits, airTubeBright,
        airLed ? airColors[airColor] : "OFF",
        airLedBright, airAuto ? "ON" : "OFF");
}

static void airDoAction(int i) {
    switch (i) {
    case 0: airStartAnimation(0); break;
    case 1: airAnimating = false; airStopSW();
            airStopwatch = false; airClock = false; airECUMode = 1;
            airCmd("DISPLAY:000000");
            strcpy(airDigits, "------"); airLastLive = 0; break;
    case 2: airAuto = !airAuto; airLastCycle = millis(); airSave(); break;
    case 3: airSetClock(); break;
    case 4: airAnimating = false; airECUMode = 0;
            airStopwatch = false; airClock = false;
            strcpy(airDigits, "------"); airDisplay("911930"); break;
    case 5: airLed = !airLed;
            airCmd(airLed ? "LED:ON" : "LED:OFF"); airSave(); break;
    case 6: airColor = (airColor + 1) % 6;
            airCmd(String("LED:COLOR:") + airColor); airSave(); break;
    case 7: airLedBright = airLedBright >= 100 ? 0 : airLedBright + 10;
            airLed = true; airCmd("LED:ON");
            airCmd(String("LED:BRIGHT:") + airLedBright); airSave(); break;
    case 8: airFade = !airFade;
            if (airFade) {
                airLed = true; airFadeVal = 0; airFadeDir = 1;
                airCmd("LED:ON"); airCmd("LED:BRIGHT:0");
            } else { airLedBright = airFadeVal; airSave(); } break;
    case 9: airTubeBright = airTubeBright >= 100 ? 0 : airTubeBright + 10;
            airCmd(String("NIXIE:BRIGHT:") + airTubeBright); airSave(); break;
    case 10: airTubeBright = airTubeBright < 10 ? 0 : airTubeBright - 10;
             airCmd(String("NIXIE:BRIGHT:") + airTubeBright); airSave(); break;
    case 11: airAnimating = false; airECUMode = 0;
             airStopwatch = false; airClock = false;
             airCmd("STARTUP:PLAY"); break;
    case 12: airBuzz = !airBuzz;
             airCmd(airBuzz ? "BUZZ:ON" : "BUZZ:OFF"); airSave(); break;
    case 13: airCmd("BUZZ:BEEP"); break;
    case 14: airCmd("PING"); airCmd("NIXIE:STATUS");
             airCmd("BUZZ:STATUS"); airCmd("LED:STATUS"); break;
    case 15: airAnimating = false; airECUMode = 0;
             airClock = false; airStopwatch = true;
             airStopSW(); airSWBase = 0; airDisplay("000000"); break;
    case 16: if (!airStopwatch) {
                 airStopwatch = true; airSWBase = 0;
                 airClock = false; airECUMode = 0; airAnimating = false;
             }
             if (airSWRun) airStopSW();
             else { airSWStart = millis(); airSWRun = true; } break;
    case 17: airStopSW(); airSWBase = 0;
             airStopwatch = true; airClock = false;
             airDisplay("000000"); break;
    case 18: airCmd("BUZZ:TONE:1500,250"); break;
    case 19: case 20: case 21: case 22: case 23:
             airAnimating = false; airStopSW();
             airStopwatch = false; airClock = false;
             airECUMode = i - 18;
             airCmd("DISPLAY:000000");
             strcpy(airDigits, "------"); airLastLive = 0; break;
    case 24: airSetClock(); break;
    case 25: airFade = false; airLed = false;
             airCmd("LED:OFF"); airSave(); break;
    case 26: open_trijekt_mode3(); break;
    case 27: airWifiStart(); break;
    case 28: airWifiStop();  break;
    }
    airUpdateStatus();
}

// AIR WERK Nixie UI: one fixed header, four category tabs and a compact
// two-column action grid.  Each tab is built only on first selection.
static lv_obj_t *airTabPages[4] = {nullptr, nullptr, nullptr, nullptr};
static lv_obj_t *airTabButtons[4] = {nullptr, nullptr, nullptr, nullptr};
static int airActiveTab = 0;
static const char *airTabNames[4] = {"DRIVE", "LIGHTING", "TOOLS", "SYSTEM"};
static const int airDriveActions[] = {0, 1, 3, 4, 19, 20, 21, 22, 23, 24};
static const int airLightActions[] = {5, 6, 7, 8, 9, 10, 25};
static const int airToolActions[] = {2, 11, 12, 13, 15, 16, 17, 18};
static const int airSystemActions[] = {14, 26, 27, 28};

static void airActionEvent(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_PRESSED) return;
    int action = (int)(intptr_t)lv_event_get_user_data(e);
    Serial.printf("NIXIE ACTION %d\n", action);
    airDoAction(action);
}

static lv_obj_t *airLabel(lv_obj_t *parent, const char *txt,
                          int x, int y, uint32_t color) {
    lv_obj_t *label = lv_label_create(parent);
    if (!label) return nullptr;
    lv_label_set_text(label, txt);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    return label;
}

static void airStyleButton(lv_obj_t *btn, bool isBack = false) {
    lv_obj_set_style_radius(btn, 12, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(isBack ? 0x382419 : 0x1A2028), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x633A1E), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(btn, lv_color_hex(isBack ? 0xE9984C : 0x39434D), 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
}

static void airAddButton(lv_obj_t *parent, int action, int col, int row) {
    lv_obj_t *btn = lv_btn_create(parent);
    if (!btn) return;
    lv_obj_set_pos(btn, 12 + col * 379, 12 + row * 79);
    lv_obj_set_size(btn, 365, 66);
    airStyleButton(btn);
    if (action == 0 || action == 26)
        lv_obj_set_style_border_color(btn, lv_color_hex(0xE9984C), 0);
    lv_obj_add_event_cb(btn, airActionEvent, LV_EVENT_PRESSED,
                        (void *)(intptr_t)action);
    lv_obj_t *label = lv_label_create(btn);
    if (!label) return;
    lv_obj_set_width(label, 337);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0xF0E9E0), 0);
    lv_label_set_text(label, airActions[action]);
    lv_obj_center(label);
}

static void airBuildTab(int tab) {
    if (airTabPages[tab] || !airList) return;
    lv_obj_t *page = lv_obj_create(airList);
    if (!page) return;
    airTabPages[tab] = page;
    lv_obj_set_pos(page, 0, 0);
    lv_obj_set_size(page, 800, 291);
    lv_obj_set_style_pad_all(page, 0, 0);
    lv_obj_set_style_border_width(page, 0, 0);
    lv_obj_set_style_radius(page, 0, 0);
    lv_obj_set_style_bg_opa(page, LV_OPA_TRANSP, 0);
    lv_obj_set_scroll_dir(page, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(page, LV_SCROLLBAR_MODE_ACTIVE);
    const int *actions = nullptr;
    int count = 0;
    switch (tab) {
        case 0: actions = airDriveActions; count = sizeof(airDriveActions)/sizeof(int); break;
        case 1: actions = airLightActions; count = sizeof(airLightActions)/sizeof(int); break;
        case 2: actions = airToolActions; count = sizeof(airToolActions)/sizeof(int); break;
        default: actions = airSystemActions; count = sizeof(airSystemActions)/sizeof(int); break;
    }
    for (int i = 0; i < count; ++i) airAddButton(page, actions[i], i % 2, i / 2);
    // Ensure lower rows are reachable when the grid exceeds the viewport.
    // Layout updated by LVGL automatically.
    lv_obj_add_flag(page, LV_OBJ_FLAG_HIDDEN);
}

static void airSelectTab(int tab) {
    if (tab < 0 || tab >= 4) return;
    airBuildTab(tab);
    if (!airTabPages[tab]) return;
    airActiveTab = tab;
    for (int i = 0; i < 4; ++i) {
        if (airTabPages[i]) {
            if (i == tab) lv_obj_clear_flag(airTabPages[i], LV_OBJ_FLAG_HIDDEN);
            else lv_obj_add_flag(airTabPages[i], LV_OBJ_FLAG_HIDDEN);
        }
        if (airTabButtons[i]) {
            lv_obj_set_style_bg_color(airTabButtons[i],
                lv_color_hex(i == tab ? 0xA65322 : 0x1C242D), 0);
            lv_obj_set_style_border_color(airTabButtons[i],
                lv_color_hex(i == tab ? 0xE9984C : 0x34404A), 0);
        }
    }
    Serial.printf("NIXIE TAB %d\n", tab);
}

static void airSelectTabAsync(void *user_data) {
    int tab = (int)(intptr_t)user_data;
    Serial.printf("NIXIE TAB REQUEST %d\\n", tab);
    airSelectTab(tab);
}

static void airTabEvent(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_PRESSED) return;

    int tab = (int)(intptr_t)lv_event_get_user_data(e);
    Serial.printf("NIXIE TAB PRESSED %d\n", tab);

    lv_async_call(airSelectTabAsync, (void *)(intptr_t)tab);
}

static void airBackEvent(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_PRESSED) open_trijekt_mode1();
}

static void airCreateScreen() {
    if (airScreen || !objects.main) return;
    airScreen = lv_obj_create(objects.main);
    if (!airScreen) return;
    lv_obj_set_pos(airScreen, 0, 0);
    lv_obj_set_size(airScreen, 800, 480);
    lv_obj_clear_flag(airScreen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(airScreen, 0, 0);
    lv_obj_set_style_border_width(airScreen, 0, 0);
    lv_obj_set_style_radius(airScreen, 0, 0);
    lv_obj_set_style_bg_color(airScreen, lv_color_hex(0x0B1016), 0);
    lv_obj_set_style_bg_opa(airScreen, LV_OPA_COVER, 0);

    // Compact back control, deliberately independent of the tab content.
    lv_obj_t *back = lv_btn_create(airScreen);
    if (back) {
        lv_obj_set_pos(back, 18, 16);
        lv_obj_set_size(back, 160, 51);
        airStyleButton(back, true);
        lv_obj_add_event_cb(back, airBackEvent, LV_EVENT_PRESSED, nullptr);
        lv_obj_t *bl = lv_label_create(back);
        if (bl) {
            lv_label_set_text(bl, "< DASHBOARD");
            lv_obj_set_style_text_color(bl, lv_color_hex(0xFFE1BF), 0);
            lv_obj_center(bl);
        }
    }
    airLabel(airScreen, "AIR WERK  /  NIXIE CONTROL", 206, 17, 0xF2B46D);
    airLabel(airScreen, "CLASSIC INSTRUMENTS  -  LIVE CONTROL", 207, 43, 0x7F8A95);
    lv_obj_t *rule = lv_obj_create(airScreen);
    if (rule) {
        lv_obj_set_pos(rule, 18, 78);
        lv_obj_set_size(rule, 764, 2);
        lv_obj_set_style_bg_color(rule, lv_color_hex(0x9A572A), 0);
        lv_obj_set_style_bg_opa(rule, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(rule, 0, 0);
    }
    airStatus = lv_label_create(airScreen);
    if (airStatus) {
        lv_obj_set_pos(airStatus, 22, 91);
        lv_obj_set_width(airStatus, 756);
        lv_label_set_long_mode(airStatus, LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_color(airStatus, lv_color_hex(0xD8DCE0), 0);
        lv_label_set_text(airStatus, "NIXIE CONTROL READY");
    }
    for (int i = 0; i < 4; ++i) {
        lv_obj_t *tab = lv_btn_create(airScreen);
        airTabButtons[i] = tab;
        if (!tab) continue;
        lv_obj_set_pos(tab, 18 + i * 194, 130);
        lv_obj_set_size(tab, 182, 47);
        airStyleButton(tab);
        lv_obj_add_event_cb(tab, airTabEvent, LV_EVENT_ALL,
                            (void *)(intptr_t)i);
        lv_obj_t *label = lv_label_create(tab);
        if (label) {
            lv_label_set_text(label, airTabNames[i]);
            lv_obj_set_style_text_color(label, lv_color_hex(0xF4E8DC), 0);
            lv_obj_center(label);
        }
    }
    airList = lv_obj_create(airScreen);
    if (airList) {
        lv_obj_set_pos(airList, 0, 185);
        lv_obj_set_size(airList, 800, 295);
        lv_obj_set_style_pad_all(airList, 0, 0);
        lv_obj_set_style_bg_opa(airList, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(airList, 0, 0);
        lv_obj_set_style_radius(airList, 0, 0);
        lv_obj_clear_flag(airList, LV_OBJ_FLAG_SCROLLABLE);
        airSelectTab(0);
    }
    lv_obj_add_flag(airScreen, LV_OBJ_FLAG_HIDDEN);
    airUpdateStatus();
    Serial.println("AIR WERK NIXIE UI READY");
}

// ============================================================
// WIFI
// ============================================================

static void airWifiStart() {
    if (airWifiPortal || airWifiConnected) {
        Serial.println("WIFI: already active; STOP to return to ESP-NOW");
        return;
    }
    espnow_enabled = false;
    if (espnow_ready) { esp_now_deinit(); espnow_ready = false; }

    WiFi.mode(WIFI_STA);
    airWiFiManager.setConfigPortalBlocking(false);
    airWiFiManager.setConfigPortalTimeout(0);
    airWiFiManager.startConfigPortal("AIR-WERK-SETUP", "airwerk911");
    airWifiPortal = true;
    Serial.println("WIFI SETUP: SSID AIR-WERK-SETUP, pass airwerk911, open 192.168.4.1");
}

static void airWifiStop() {
    if (airWifiPortal) {
        airWiFiManager.stopConfigPortal();
        airWifiPortal = false;
    }
    WiFi.disconnect(true, false);
    WiFi.mode(WIFI_STA);
    airWifiConnected = false;
    espnow_ready = false;
    espnow_enabled = true;
    if (!espnow_init_simple()) Serial.println("ESP-NOW restore failed");
    Serial.println("WIFI stopped; ESP-NOW channel 1 restored");
}

static void airWifiTick() {
    if (!airWifiPortal) return;
    airWiFiManager.process();
    if (WiFi.status() == WL_CONNECTED) {
        airWifiConnected = true;
        airWifiPortal = false;
        Serial.printf("WIFI CONNECTED: %s\n",
                      WiFi.localIP().toString().c_str());
    }
}

// ============================================================
// AIR WERK INIT + TICK
// ============================================================

static void airInit() {
    airPrefs.begin("airwerk", false);
    airLed = airPrefs.getBool("led", false);
    airBuzz = airPrefs.getBool("buzzer", true);
    airAuto = airPrefs.getBool("autocine", false);
    airColor = airPrefs.getUChar("color", 0) % 6;
    airLedBright = airPrefs.getUChar("ledbright", 25);
    if (airLedBright > 100) airLedBright = 25;
    airTubeBright = airPrefs.getUChar("tube", 100);
    if (airTubeBright > 100) airTubeBright = 100;

    if (AIRWERK_UART_ENABLED) {
        airSerial.begin(9600, SERIAL_8N1,
                        AIRWERK_NIXIE_RX, AIRWERK_NIXIE_TX);
        Serial.printf("AIR WERK RS485 READY: TX GPIO%d RX GPIO%d 9600 8N1\n",
                      AIRWERK_NIXIE_TX, AIRWERK_NIXIE_RX);
    }

    airCmd("LED:OFF");
    airCmd(String("LED:COLOR:") + airColor);
    airCmd(String("LED:BRIGHT:") + airLedBright);
    airCmd(String("NIXIE:BRIGHT:") + airTubeBright);
    airCmd(airBuzz ? "BUZZ:ON" : "BUZZ:OFF");
    airCmd("DOTS:ON");
    airSetClock();
}

static void airTick() {
    uint32_t now = millis();

    static char rx[96];
    static uint8_t n = 0;
    while (AIRWERK_UART_ENABLED && airSerial.available()) {
        char c = (char)airSerial.read();
        if (c == '\n') {
            rx[n] = 0;
            if (n) Serial.printf("AIR WERK MEGA: %s\n", rx);
            n = 0;
        } else if (c != '\r') {
            if (n < sizeof(rx) - 1) rx[n++] = c;
            else n = 0;
        }
    }

    if (airAuto && airClock && !airAnimating &&
        now - airLastCycle >= 300000UL) {
        airStartAnimation(0);
    }

    if (airAnimating) {
        uint32_t t = now - airAnimStart;
        if (t >= 12000UL) {
            airAnimating = false;
            airCmd(String("NIXIE:BRIGHT:") + airTubeBright);
            airCmd("LED:OFF");
            airSetClock();
        } else if (t >= 9600UL) {
            airDisplay("911930");
        } else if (now - airLastFrame >= 85) {
            airLastFrame = now;
            char d[7] = "000000";
            const char target[] = "911930";
            const bool kittPhase = (t >= 4800UL);
            const uint32_t phaseT = kittPhase ? (t - 4800UL) : t;

            if (kittPhase) {
                uint8_t pct = 100;
                if (phaseT < 1150) {
                    uint16_t ph = phaseT % 180;
                    pct = 42 + (ph < 90 ? ph * 58 / 90 : (180 - ph) * 58 / 90);
                } else if (phaseT < 2300) {
                    pct = ((phaseT / 90) % 2) ? 100 : 48;
                } else if (phaseT < 3050) {
                    pct = ((phaseT / 120) % 2) ? 68 : 35;
                } else if (phaseT < 3650) {
                    pct = ((phaseT / 130) % 2) ? 100 : 28;
                } else if (phaseT < 3890) {
                    pct = ((phaseT / 80) % 2) ? 18 : 100;
                } else if (phaseT < 4550) {
                    pct = 25 + (phaseT - 3890) * 75 / 660;
                }
                int actual = airTubeBright * pct / 100;
                if (actual != airLastPulse) {
                    airCmd(String("NIXIE:BRIGHT:") + actual);
                    airLastPulse = actual;
                }
                if (phaseT < 3050) {
                    uint32_t step = phaseT < 1150 ? phaseT / 180
                                  : phaseT < 2300 ? (phaseT - 1150) / 95
                                                  : (phaseT - 2300) / 95;
                    uint8_t phase = step % 6;
                    uint8_t pos = phase <= 3 ? phase : 6 - phase;
                    d[pos] = '9'; d[pos+1] = '1'; d[pos+2] = '1';
                } else if (phaseT < 3650) {
                    int pos = ((phaseT - 3050) / 130) % 2 ? 3 : 0;
                    d[pos] = '9'; d[pos+1] = '1'; d[pos+2] = '1';
                } else if (phaseT < 3890) {
                    if (((phaseT - 3650) / 80) % 2 == 0) strcpy(d, "911911");
                } else if (phaseT < 4110) strcpy(d, "900000");
                else if (phaseT < 4330)   strcpy(d, "911000");
                else                      strcpy(d, target);
            } else {
                if (phaseT < 800) {
                    int step = phaseT / 85;
                    d[0] = '0' + step % 10;
                    d[5] = '0' + (10 - step % 10) % 10;
                } else if (phaseT < 1550) {
                    int step = (phaseT - 800) / 85;
                    d[0] = '0' + (step + 3) % 10;
                    d[5] = '0' + (step + 7) % 10;
                    d[1] = '0' + (step + 5) % 10;
                    d[4] = '0' + (step + 2) % 10;
                } else if (phaseT < 3900) {
                    int step = (phaseT - 1550) / (phaseT < 2450 ? 65 : 85);
                    for (int j = 0; j < 6; j++)
                        d[j] = '0' + (step * (j % 2 ? 3 : 7) + j * 3) % 10;
                    if (phaseT >= 2450) { d[0] = '9'; d[5] = '0'; }
                    if (phaseT >= 2900) { d[1] = '1'; d[4] = '3'; }
                    if (phaseT >= 3450) { d[2] = '1'; d[3] = '9'; }
                } else {
                    strcpy(d, target);
                }
            }
            airDisplay(d);
        }
    }

    if (airECUMode && now - airLastLive >= 250) {
        airLastLive = now;
        float value = NAN;
        bool ok = false;
        switch (airECUMode) {
        case 1: ok = fresh(trijekt.rpmValid, trijekt.lastRpmMs, RPM_TIMEOUT_MS);
                value = trijekt.rpm; break;
        case 2: ok = fresh(trijekt.batteryValid, trijekt.lastBatteryMs);
                value = trijekt.battery * 100; break;
        case 3: ok = fresh(trijekt.tempValid, trijekt.lastTempMs);
                value = trijekt.engineTemp * 10; break;
        case 4: ok = fresh(trijekt.throttleValid, trijekt.lastThrottleMs);
                value = trijekt.throttleDK * 10; break;
        case 5: ok = fresh(trijekt.throttleValid, trijekt.lastThrottleMs);
                value = trijekt.throttlePedal * 10; break;
        }
        if (ok && isfinite(value) && value >= 0 && value <= 999999) {
            char d[7];
            if (airECUMode == 1)
                snprintf(d, sizeof(d), "%04lu00",
                         (unsigned long)(value + 0.5f));
            else
                snprintf(d, sizeof(d), "%06lu",
                         (unsigned long)(value + 0.5f));
            airDisplay(d);
        } else if (strcmp(airDigits, "------") != 0) {
            airDisplay("000000");
            strcpy(airDigits, "------");
        }
    }

    if (airStopwatch && now - airLastStop >= 40) {
        airLastStop = now;
        uint32_t cs = (airSWMillis() / 10) % 600000UL;
        char d[7];
        snprintf(d, sizeof(d), "%02lu%02lu%02lu",
                 (unsigned long)(cs / 6000),
                 (unsigned long)((cs / 100) % 60),
                 (unsigned long)(cs % 100));
        airDisplay(d);
    }

    if (airFade && now - airLastFade >= 35) {
        airLastFade = now;
        airFadeVal += airFadeDir * 2;
        if (airFadeVal >= 100) { airFadeVal = 100; airFadeDir = -1; }
        if (airFadeVal <= 0)   { airFadeVal = 0;   airFadeDir = 1; }
        airCmd(String("LED:BRIGHT:") + airFadeVal);
    }

    static uint32_t lastStatus = 0;
    if (now - lastStatus >= 500) {
        lastStatus = now;
        lvgl_port_lock(-1);
        airUpdateStatus();
        lvgl_port_unlock();
    }
}

// ============================================================
// SETUP
// ============================================================

void setup() {
    Serial.begin(115200);
    delay(300);

    Serial.println();
    Serial.println("====================================");
    Serial.println("WAVESHARE TRIJEKT PREMIUM 3.0");
    Serial.println("MODE 1 / MODE 2 / MODE 3");
    Serial.println("====================================");

    ecu = ECUData{};
    trijekt = TrijektValues{};

    board = new Board();
    board->init();

#if LVGL_PORT_AVOID_TEARING_MODE
    {
        auto lcd = board->getLCD();
        lcd->configFrameBufferNumber(LVGL_PORT_DISP_BUFFER_NUM);
    }
#endif

    assert(board->begin());
    lvgl_port_init(board->getLCD(), board->getTouch());

    lvgl_port_lock(-1);

    ui_init();
    originalScreen = objects.main ? objects.main : lv_scr_act();

    Serial.printf("BOOT: originalScreen=%p objects.main=%p\n",
                  (void *)originalScreen, (void *)objects.main);

    // Secondary panels are created on demand by navigateToPage().

    // Navigation is owned by screens.c. Do not attach a second
    // MODE 2 handler, which would override the ECU destination.

    if (originalScreen) lv_scr_load(originalScreen);

    lvgl_port_unlock();

    Serial.println("LVGL UI READY");

    airInit();

    can_ok = waveshare_twai_init();
    Serial.println(can_ok ? "CAN INITIALIZED" : "CAN INIT FAILED");

    Serial.println(espnow_init_simple() ? "ESP-NOW INITIALIZED"
                                        : "ESP-NOW FAILED");
    Serial.println("WAITING FOR TRIJEKT CAN DATA");
}

// ============================================================
// MAIN LOOP
// ============================================================

void loop() {
    uint32_t now = millis();

    // 1. CAN
    if (can_ok) {
        twai_message_t msg;
        const uint32_t start = micros();
        for (int i = 0; i < CAN_BATCH_SIZE; i++) {
            if ((uint32_t)(micros() - start) >= CAN_TIME_BUDGET_US) break;
            if (twai_receive(&msg, 0) != ESP_OK) break;
            recordCANFrame(msg);
            decodeCANMessage(msg);
        }
    }

    // 2. HOUSEKEEPING
    updateECUHousekeeping();
    updateDashboardDemo();

    // 3. LVGL
    static uint32_t lastUI = 0;
    now = millis();
    if (now - lastUI >= UI_INTERVAL_MS) {
        lvgl_port_lock(-1);

        processNavigationRequest();

        if (lv_scr_act() == originalScreen) {
            update_mock_rpm();
            update_mock_throttle();
            update_mock_lambda();
            update_mock_motor_temp();
        }
        if (currentPage == 2) {
            updateCANScreen();
        }

        lvgl_port_unlock();
        lastUI = millis();
    }

    // 4. ESP-NOW
    if (!DASH_DEMO_MODE) sendEspNowValues();

    // 5. Live debug
    static uint32_t lastDebug = 0;
    if (PRINT_LIVE_VALUES && millis() - lastDebug >= DEBUG_INTERVAL_MS) {
        printAllValues();
        lastDebug = millis();
    }

    // 6. CAN diagnostics
    if (PRINT_CAN_DIAGNOSTICS &&
        millis() - canLastReportMs >= CAN_DEBUG_INTERVAL_MS) {
        printCANDiagnostics();
    }

    // 7. Nixie / WiFi
    airTick();
    airWifiTick();

    delay(1);
}