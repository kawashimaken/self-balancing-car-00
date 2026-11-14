#include <Arduino.h>
#include <Wire.h>
#include <SimpleFOC.h>
#include <Adafruit_BNO055.h>
#include <Adafruit_Sensor.h>
#include "config.h"
#include "shared_state.h"
#include "web_ui.h"

// MKS ESP32 FOC V1.0: BNO055 + AS5600 x2 + BLDC x2。
// Core 1: FOC・姿勢制御、Core 0: Wi-Fi Web UI。

// ハード定義
BLDCMotor      motorR  = BLDCMotor(POLE_PAIRS);
BLDCDriver3PWM driverR = BLDCDriver3PWM(32, 33, 25, 22);
BLDCMotor      motorL  = BLDCMotor(POLE_PAIRS);
BLDCDriver3PWM driverL = BLDCDriver3PWM(26, 27, 14, 12);

TwoWire I2Cone = TwoWire(0);
TwoWire I2Ctwo = TwoWire(1);
MagneticSensorI2C sensorR = MagneticSensorI2C(AS5600_I2C);
MagneticSensorI2C sensorL = MagneticSensorI2C(AS5600_I2C);
Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x29, &I2Cone);

LowPassFilter lpf_vel{0.07f};  // 車輪速度ノイズ除去（Tf=70ms）

// 制御用変数
static uint32_t last_ctrl_us  = 0;
static uint32_t last_print_ms = 0;
static float    integral      = 0.0f;
static bool     fallen        = false;

static uint32_t foc_loop_count = 0;
static uint32_t last_hz_us     = 0;

// 速度カスケード制御（外側ループ）の状態
static float target_vel  = 0.0f;  // ランプ後の目標車輪速度 [rad/s]
static float vel_int_deg = 0.0f;  // 速度I項の累積（傾き角換算 [deg]、アンチワインドアップ付き）
static float pitch_cmd   = 0.0f;  // LPF後の傾き指令 [deg]
static float vel_slow    = 0.0f;  // トリム用にLPFした車輪速（揺れ成分を遮断）[rad/s]

// 姿勢取得
static float readPitchDeg() {
  imu::Vector<3> g = bno.getVector(Adafruit_BNO055::VECTOR_GRAVITY);
  return atan2(g.x(), g.z()) * RAD_TO_DEG;
}

static float readPitchRate() {
  imu::Vector<3> w = bno.getVector(Adafruit_BNO055::VECTOR_GYROSCOPE);
  return -w.y();
}

// セットアップ
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n[Balance] boot");

  webUiBegin();  // WiFi AP 起動 + Core 0 タスク起動

  // I2C
  I2Cone.begin(19, 18, 400000);
  I2Ctwo.begin(23, 5,  400000);
  I2Cone.setTimeOut(25);

  // エンコーダ
  sensorR.init(&I2Cone);
  sensorL.init(&I2Ctwo);
  motorR.linkSensor(&sensorR);
  motorL.linkSensor(&sensorL);

  // ドライバ
  driverR.voltage_power_supply = SUPPLY_VOLTAGE;
  driverL.voltage_power_supply = SUPPLY_VOLTAGE;
  driverR.init();
  driverL.init();
  motorR.linkDriver(&driverR);
  motorL.linkDriver(&driverL);

  // 制御モード
  BLDCMotor* motors[] = { &motorR, &motorL };
  for (BLDCMotor* m : motors) {
    m->foc_modulation    = FOCModulationType::SpaceVectorPWM;
    m->controller        = MotionControlType::torque;
    m->torque_controller = TorqueControlType::voltage;
    m->voltage_limit     = MOTOR_VOLTAGE_LIMIT;
    m->velocity_limit    = VELOCITY_LIMIT_RADS;
  }

  // BNO055（加速度+ジャイロのみ：モーター磁石の干渉を避ける）
  if (!bno.begin(OPERATION_MODE_IMUPLUS)) {
    Serial.println("[ERR] BNO055 not found");
    while (1) delay(1000);
  }
  bno.setExtCrystalUse(true);
  delay(500);

  // FOC 初期化
  Serial.println("[Balance] initFOC... 車輪が回ります。車体を持ってください");
  motorR.init();  motorR.initFOC();
  motorL.init();  motorL.initFOC();

  Serial.println("[Balance] ready");
  last_ctrl_us = micros();
  last_hz_us   = micros();
}

// メインループ（Core 1）
void loop() {
  motorR.loopFOC();
  motorL.loopFOC();
  foc_loop_count++;

  uint32_t now = micros();
  if (now - last_ctrl_us < CONTROL_DT_US) return;

  float dt = (now - last_ctrl_us) * 1e-6f;
  last_ctrl_us = now;

  // ゲインと走行コマンドをローカルにコピー（この周期の計算で使用）
  float kp = Kp, ki = Ki, kd = Kd, kv = Kv, kvi = Kvi, sp = setpoint_deg;
  float vmax = DriveVmax;
  float drive = cmd_drive, turn = cmd_turn;

  // 走行コマンドタイムアウト：一定時間 /cmd が来なければ自動停止
  if (millis() - last_cmd_ms > CMD_TIMEOUT_MS) {
    cmd_drive = 0.0f; cmd_turn = 0.0f;
    drive = 0.0f;     turn  = 0.0f;
  }

  float pitch = readPitchDeg();
  float rate  = readPitchRate();
  // 車輪速度は前進を正に揃える。
  float vel   = lpf_vel(-0.5f * (DIR_R * motorR.shaft_velocity + DIR_L * motorL.shaft_velocity));

  float angle = pitch - sp;

  if (fabs(angle) > MAX_TILT_DEG) fallen = true;
  if (fallen && fabs(angle) < 5.0f) {
    fallen = false;
    integral = 0.0f; vel_int_deg = 0.0f; pitch_cmd = 0.0f; target_vel = 0.0f;
    vel_slow = 0.0f;
  }

  // 速度指令から FF と姿勢トリムを生成し、角度 PID と車輪速ダンピングで制御。

  // 目標速度ランプ：drive 指令を加速度制限付きで目標速度 [rad/s] へ変換
  float vel_cmd = drive * vmax;
  target_vel += constrain(vel_cmd - target_vel,
                          -DRIVE_ACCEL_RADS2 * dt, DRIVE_ACCEL_RADS2 * dt);

  float u = 0.0f, turn_v = 0.0f;
  if (!fallen) {
    // FF は目標速度に必要な傾きを与える。トリムは低速ドリフトを補正。
    // 前進方向の車輪速に正の傾き補正を加える。
    float pitch_ff = (kp > 0.05f) ? (KV_DAMP_V_PER_RADS * target_vel / kp) : 0.0f;
    float vel_err  = vel - target_vel;   // 「流れている向き」が正

    // 停止指令中かつ低角速度・低車輪速のときだけ、平滑化した車輪速を積分。
    vel_slow += (vel - vel_slow) * dt / (VEL_TRIM_LPF_TF + dt);
    bool quiet = (fabs(rate) < TRIM_QUIET_RATE_DPS) && (fabs(vel) < TRIM_QUIET_VEL_RADS);
    if (fabs(target_vel) < 0.5f && quiet) {
      vel_int_deg += kvi * vel_slow * dt;
      vel_int_deg  = constrain(vel_int_deg, -VEL_TRIM_LIMIT_DEG, VEL_TRIM_LIMIT_DEG);
    }
    float pitch_cmd_raw = constrain(pitch_ff + kv * vel_err + vel_int_deg,
                                    -VEL_PITCH_LIMIT_DEG, VEL_PITCH_LIMIT_DEG);
    pitch_cmd += (pitch_cmd_raw - pitch_cmd) * dt / (VEL_PITCH_LPF_TF + dt);

    // 角度 PID + 絶対車輪速ダンピング。速度誤差に変更すると FF の発進効果を相殺する。
    float ctrl_angle = pitch - (sp + pitch_cmd);
    integral += ctrl_angle * dt;
    integral  = constrain(integral, -10.0f, 10.0f);
    u = kp*ctrl_angle + ki*integral + kd*rate - KV_DAMP_V_PER_RADS*vel;
    u = constrain(u, -MOTOR_VOLTAGE_LIMIT, MOTOR_VOLTAGE_LIMIT);
    // 旋回：共通加算（右+、左+）→ DIR補正後は両輪が逆方向 → 旋回
    turn_v = constrain(turn * TURN_VOLTAGE_SCALE, -MOTOR_VOLTAGE_LIMIT, MOTOR_VOLTAGE_LIMIT);
  }

  motorR.move(constrain(DIR_R * u + turn_v, -MOTOR_VOLTAGE_LIMIT, MOTOR_VOLTAGE_LIMIT));
  motorL.move(constrain(DIR_L * u + turn_v, -MOTOR_VOLTAGE_LIMIT, MOTOR_VOLTAGE_LIMIT));

  telem_pitch     = pitch;
  telem_rate      = rate;
  telem_u         = u;
  telem_vel       = vel;
  telem_pitch_cmd = pitch_cmd;

  // FOC ループ周波数を 1 秒ごとに計算
  if (now - last_hz_us >= 1000000UL) {
    telem_foc_hz   = foc_loop_count * 1000000.0f / (now - last_hz_us);
    foc_loop_count = 0;
    last_hz_us     = now;
  }

  uint32_t now_ms = millis();
  if (now_ms - last_print_ms >= 200) {
    last_print_ms = now_ms;
    Serial.printf("pitch=%6.2f  pcmd=%+5.2f  u=%6.3f  vel=%6.1f  tvel=%+5.1f turn=%+.1f  foc=%.0fHz\n",
                  pitch, pitch_cmd, u, vel, target_vel, turn, telem_foc_hz);
  }

}
