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

  // ゲインをローカルにコピー（この周期の計算で使用）
  float kp = Kp, ki = Ki, kd = Kd, kv = Kv, sp = setpoint_deg;

  float pitch = readPitchDeg();
  float rate  = readPitchRate();
  float angle = pitch - sp;
  // 車輪速度は前進を正に揃える。
  float vel   = lpf_vel(-0.5f * (DIR_R * motorR.shaft_velocity + DIR_L * motorL.shaft_velocity));

  if (fabs(angle) > MAX_TILT_DEG) fallen = true;
  if (fallen && fabs(angle) < 5.0f) { fallen = false; integral = 0.0f; }

  float u = 0.0f;
  if (!fallen) {
    integral += angle * dt;
    integral  = constrain(integral, -10.0f, 10.0f);
    u = kp*angle + ki*integral + kd*rate - kv*vel;
    u = constrain(u, -MOTOR_VOLTAGE_LIMIT, MOTOR_VOLTAGE_LIMIT);
  }

  motorR.move(DIR_R * u);
  motorL.move(DIR_L * u);

  telem_pitch = pitch;
  telem_rate  = rate;
  telem_u     = u;
  telem_vel   = vel;

  // FOC ループ周波数を 1 秒ごとに計算
  if (now - last_hz_us >= 1000000UL) {
    telem_foc_hz   = foc_loop_count * 1000000.0f / (now - last_hz_us);
    foc_loop_count = 0;
    last_hz_us     = now;
  }

  uint32_t now_ms = millis();
  if (now_ms - last_print_ms >= 200) {
    last_print_ms = now_ms;
    Serial.printf("pitch=%6.2f  rate=%6.1f  u=%6.3f  vel=%6.1f  Kp=%.3f Kd=%.4f  foc=%.0fHz\n",
                  pitch, rate, u, vel, kp, kd, telem_foc_hz);
  }

}
