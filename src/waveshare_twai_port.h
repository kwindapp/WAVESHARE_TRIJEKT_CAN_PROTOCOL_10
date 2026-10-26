#ifndef __TWAI_PORT_H
#define __TWAI_PORT_H

#pragma once

#include <Arduino.h>
#include <driver/twai.h>

// Waveshare ESP32-S3 CAN transceiver
#define RX_PIN 16
#define TX_PIN 15

#define POLLING_RATE_MS 1000

bool waveshare_twai_init();
void waveshare_twai_receive();

#endif