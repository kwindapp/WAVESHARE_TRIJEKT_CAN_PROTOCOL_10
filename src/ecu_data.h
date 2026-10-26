#pragma once
#include <stdint.h>
#include <stdbool.h>  // Add this for bool type

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t rpm;
    float    batt;
    float    motor;
    float    throttle_dk;
    float    throttle_pedal;
    float    lambda;
    uint8_t  lambda_valid;
    uint16_t errors;
    uint8_t  data_valid;
    uint32_t last_update;
    bool     engine_running;
    float    fuel_temp;
    float    air_temp;
    uint16_t manifold_pressure;
} ECUData;

extern ECUData  ecu;
extern uint16_t rpm_max;

#ifdef __cplusplus
}
#endif