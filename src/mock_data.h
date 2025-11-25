#ifndef MOCK_DATA_H
#define MOCK_DATA_H

#ifdef __cplusplus
extern "C" {
#endif

void update_mock_rpm(void);
void update_mock_throttle(void);     // Batt
void update_mock_lambda(void);       // Lambda
void update_mock_motor_temp(void);   // Temp

#ifdef __cplusplus
}
#endif

#endif // MOCK_DATA_H
