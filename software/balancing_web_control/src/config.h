#pragma once
#include <stdint.h>

// モーター設定
static const int   POLE_PAIRS          = 7;
static const float SUPPLY_VOLTAGE      = 12.0f;
// モーター電圧の上限 [V]。引き上げる場合は発熱に注意。
static const float MOTOR_VOLTAGE_LIMIT = 5.5f;
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

// 走行制御スケール
// 目標速度は Web UI の Vmax で設定。FF と姿勢トリムから傾き指令を生成。
// VEL_PITCH_LIMIT_DEG は最大速度の FF とトリムを含めて設定する。
static const float DRIVE_ACCEL_RADS2   = 30.0f;  // [rad/s^2] 目標速度のランプ加速度
static const float VEL_PITCH_LIMIT_DEG = 14.0f;  // [deg] 傾き指令全体（FF+トリム）の上限
static const float VEL_PITCH_LPF_TF    = 0.05f;  // [s] 傾き指令の1次LPF時定数
// 釣り合い角のずれを補正するトリム上限 [deg]。増やしすぎると走り去る恐れがある。
static const float VEL_TRIM_LIMIT_DEG  = 2.0f;
// トリム用の車輪速 LPF 時定数 [s]。バランス揺れを除き、低速ドリフトを抽出。
static const float VEL_TRIM_LPF_TF     = 0.4f;
// 外乱回復中の積分を避けるため、低角速度・低車輪速のときだけトリムを更新。
static const float TRIM_QUIET_RATE_DPS  = 40.0f;  // [deg/s] 体の角速度がこれ未満
static const float TRIM_QUIET_VEL_RADS  = 2.5f;   // [rad/s] 車輪速がこれ未満
// 絶対車輪速度に対するダンピング係数 [V/(rad/s)]。走行時の定常分は FF で補償。
static const float KV_DAMP_V_PER_RADS  = 0.19f;
// 旋回：共通加算電圧（右+、左+ → 両輪が逆の物理方向 → 旋回）
// 向きが逆の場合は TURN_VOLTAGE_SCALE をマイナスにする
static const float    TURN_VOLTAGE_SCALE = 0.5f;   // [V / unit]
static const uint32_t CMD_TIMEOUT_MS      = 500;    // [ms] コマンド無受信で自動停止
