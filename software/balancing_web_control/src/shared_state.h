#pragma once
#include <stdint.h>

// Core 0（Wi-Fi）と Core 1（制御ループ）の共有変数。

// PID ゲイン（Core 0 の WiFi タスクが書き込む）
extern volatile float Kp;
extern volatile float Ki;
extern volatile float Kd;
extern volatile float Kv;    // 傾き補正 P [deg/(rad/s)]
extern volatile float Kvi;   // 姿勢トリム I [deg/rad]
extern volatile float DriveVmax;  // drive=±1 時の目標車輪速度 [rad/s]
extern volatile float setpoint_deg;

// テレメトリ（Core 1 の FOC ループが書き込む）
extern volatile float telem_pitch;
extern volatile float telem_rate;
extern volatile float telem_u;
extern volatile float telem_vel;       // 平均車輪速度 [rad/s]
extern volatile float telem_pitch_cmd; // 外側速度ループが生成した傾き指令 [deg]
extern volatile float telem_foc_hz;

// Web UI からの走行コマンド
// cmd_drive: -1.0(後退) .. +1.0(前進)
// cmd_turn:  -1.0(左旋回) .. +1.0(右旋回)
// last_cmd_ms: 最終受信時刻 [ms]。タイムアウト時は走行指令をゼロにする。
extern volatile float    cmd_drive;
extern volatile float    cmd_turn;
extern volatile uint32_t last_cmd_ms;
