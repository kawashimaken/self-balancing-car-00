#include "shared_state.h"

// PID ゲイン初期値
volatile float Kp          = 0.27f;
volatile float Ki          = 0.006f;
volatile float Kd          = 0.021f;
// Kv は傾き指令の P 項（通常は 0）、Kvi は低速ドリフト補正の積分ゲイン。
volatile float Kv          = 0.0f;   // 外側ループ P [deg/(rad/s)]（基本 0）
volatile float Kvi         = 0.5f;   // 自動 sp 校正トリム [deg/rad]
volatile float DriveVmax   = 7.0f;   // drive=±1 時の目標車輪速度 [rad/s]（Web で調整）
volatile float setpoint_deg = 0.5f;

// テレメトリ初期値
volatile float telem_pitch  = 0.0f;
volatile float telem_rate   = 0.0f;
volatile float telem_u      = 0.0f;
volatile float telem_vel       = 0.0f;
volatile float telem_pitch_cmd = 0.0f;
volatile float telem_foc_hz    = 0.0f;

// 走行コマンド初期値
volatile float    cmd_drive   = 0.0f;
volatile float    cmd_turn    = 0.0f;
volatile uint32_t last_cmd_ms = 0;
