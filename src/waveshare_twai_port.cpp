#include "waveshare_twai_port.h"

#include <Arduino.h>
#include <driver/twai.h>

// =====================================================
// WAVESHARE ESP32-S3 / TRIJEKT PREMIUM 3.0
//
// CAN RX: GPIO 16
// CAN TX: GPIO 15
// CAN speed: 500 kbps
// CAN mode: NORMAL (ACK enabled)
//
// main.cpp handles incoming CAN frames.
// =====================================================

static bool twai_initialized = false;

// =====================================================
// INITIALIZE CAN
// =====================================================

bool waveshare_twai_init()
{
    if (twai_initialized)
    {
        Serial.println("TWAI already initialized");
        return true;
    }

    // NORMAL MODE: acknowledge received CAN frames
    twai_general_config_t g_config =
        TWAI_GENERAL_CONFIG_DEFAULT(
            (gpio_num_t)TX_PIN,
            (gpio_num_t)RX_PIN,
            TWAI_MODE_NORMAL
        );

    // Increase RX queue
    g_config.rx_queue_len = 1000;

    // No application CAN transmissions
    g_config.tx_queue_len = 0;

    // Trijekt CAN speed
    twai_timing_config_t t_config =
        TWAI_TIMING_CONFIG_500KBITS();

    // Receive all CAN identifiers
    twai_filter_config_t f_config =
        TWAI_FILTER_CONFIG_ACCEPT_ALL();

    // Install driver
    esp_err_t result = twai_driver_install(
        &g_config,
        &t_config,
        &f_config
    );

    if (result != ESP_OK)
    {
        Serial.printf(
            "TWAI driver installation failed: %s\n",
            esp_err_to_name(result)
        );
        return false;
    }

    // Start CAN controller
    result = twai_start();

    if (result != ESP_OK)
    {
        Serial.printf(
            "TWAI start failed: %s\n",
            esp_err_to_name(result)
        );

        twai_driver_uninstall();
        return false;
    }

    // Enable diagnostics
    uint32_t alerts =
        TWAI_ALERT_ERR_PASS |
        TWAI_ALERT_BUS_ERROR |
        TWAI_ALERT_RX_QUEUE_FULL |
        TWAI_ALERT_BUS_OFF;

    result = twai_reconfigure_alerts(alerts, NULL);

    if (result != ESP_OK)
    {
        Serial.printf(
            "TWAI alert configuration failed: %s\n",
            esp_err_to_name(result)
        );
    }

    twai_initialized = true;

    Serial.println();
    Serial.println("===================================");
    Serial.println("TRIJEKT CAN DRIVER INITIALIZED");
    Serial.println("===================================");
    Serial.println("CAN SPEED : 500 kbps");
    Serial.println("CAN RX    : GPIO 16");
    Serial.println("CAN TX    : GPIO 15");
    Serial.println("CAN MODE  : NORMAL");
    Serial.println("RX QUEUE  : 1000");
    Serial.println("CAN ACK   : ENABLED");
    Serial.println("===================================");

    return true;
}

// =====================================================
// RECEIVE FUNCTION
//
// main.cpp already calls twai_receive() directly.
// Do not consume the same CAN frames here.
// =====================================================

void waveshare_twai_receive()
{
    // Intentionally empty.
    // main.cpp handles CAN reception and decoding.
}