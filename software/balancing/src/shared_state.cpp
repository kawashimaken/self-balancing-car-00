#include "shared_state.h"

// PID ゲイン初期値
volatile float Kp          = 0.23f;
volatile float Ki          = 0.006f;
volatile float Kd          = 0.021f;
volatile float Kv          = 0.22f;  // 車輪速フィードバックでドリフトを抑制
volatile float setpoint_deg = 0.8f;

// テレメトリ初期値
volatile float telem_pitch  = 0.0f;
volatile float telem_rate   = 0.0f;
volatile float telem_u      = 0.0f;
volatile float telem_vel    = 0.0f;
volatile float telem_foc_hz = 0.0f;
