#pragma once

// Core 0（Wi-Fi）と Core 1（制御ループ）の共有変数。

// PID ゲイン（Core 0 の WiFi タスクが書き込む）
extern volatile float Kp;
extern volatile float Ki;
extern volatile float Kd;
extern volatile float Kv;
extern volatile float setpoint_deg;

// テレメトリ（Core 1 の FOC ループが書き込む）
extern volatile float telem_pitch;
extern volatile float telem_rate;
extern volatile float telem_u;
extern volatile float telem_vel;    // 平均車輪速度 [rad/s]
extern volatile float telem_foc_hz;
