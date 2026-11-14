#pragma once
#include <stdint.h>

// モーター設定
static const int   POLE_PAIRS          = 7;
static const float SUPPLY_VOLTAGE      = 12.0f;
static const float MOTOR_VOLTAGE_LIMIT = 4.0f;
static const float MAX_TILT_DEG        = 30.0f;

static const float DIR_R = -1.0f;
static const float DIR_L = +1.0f;

// 車輪速度の上限 [rad/s]。
static const float VELOCITY_LIMIT_RADS = 200.0f;

// 制御周期
// 姿勢制御は BNO055 の出力に合わせて 100 Hz。FOC は毎ループ実行。
static const uint32_t CONTROL_DT_US = 10000; // 100Hz

// WiFi 設定
#include "wifi_config.h"  // Copy wifi_config.example.h and set your own AP credentials.
