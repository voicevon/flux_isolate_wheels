#include "AppProduction.h"
#include "MotorHardware.h"
#include "ShiftRegisterBus.h"
#include "MqttLink.h"
#include "config.h"
#include "pins.h"
#include "Logger.h"
#include <Arduino.h>

// 全局唯一实例指针，供 MQTT 命令回调中转
static AppProduction* s_production = NULL;

// --- 简易扁平 JSON 字段提取（无需完整解析器） ---

// 提取字符串字段值（"key":"value"），成功返回 true
static bool jsonGetString(const String& s, const char* key, String& out) {
  int k = s.indexOf(String("\"") + key + "\"");
  if (k < 0) return false;
  int colon = s.indexOf(':', k);
  if (colon < 0) return false;
  int q1 = s.indexOf('"', colon);
  if (q1 < 0) return false;
  int q2 = s.indexOf('"', q1 + 1);
  if (q2 < 0) return false;
  out = s.substring(q1 + 1, q2);
  return true;
}

// 提取数值字段值（"key":123 / "key":22.5），成功返回 true
static bool jsonGetFloat(const String& s, const char* key, float& out) {
  int k = s.indexOf(String("\"") + key + "\"");
  if (k < 0) return false;
  int colon = s.indexOf(':', k);
  if (colon < 0) return false;
  int p = colon + 1;
  while (p < (int)s.length() && (s[p] == ' ' || s[p] == '\t')) p++;
  int start = p;
  if (p < (int)s.length() && s[p] == '-') p++;
  bool hasDigit = false, hasDot = false;
  while (p < (int)s.length()) {
    char c = s[p];
    if (c >= '0' && c <= '9') { p++; hasDigit = true; }
    else if (c == '.' && !hasDot) { p++; hasDot = true; }
    else break;
  }
  if (!hasDigit) return false;
  out = s.substring(start, p).toFloat();
  return true;
}

// 提取数值数组字段（"key":[v1,...,v8]，支持负数与小数），填满 n 个返回 true
static bool jsonGetFloatArray(const String& s, const char* key, float* out, int n) {
  int k = s.indexOf(String("\"") + key + "\"");
  if (k < 0) return false;
  int arrStart = s.indexOf('[', k);
  int arrEnd = s.indexOf(']', arrStart);
  if (arrStart < 0 || arrEnd < 0) return false;

  String arr = s.substring(arrStart + 1, arrEnd);
  int idx = 0, p = 0, len = arr.length();
  while (p < len && idx < n) {
    // 跳过到数字起始（负号/数字）
    while (p < len && arr[p] != '-' && (arr[p] < '0' || arr[p] > '9')) p++;
    if (p >= len) break;
    int start = p;
    if (arr[p] == '-') p++;
    bool hasDigit = false, hasDot = false;
    while (p < len) {
      char c = arr[p];
      if (c >= '0' && c <= '9') { p++; hasDigit = true; }
      else if (c == '.' && !hasDot) { p++; hasDot = true; }
      else break;
    }
    if (!hasDigit) break;
    out[idx++] = arr.substring(start, p).toFloat();
  }
  return idx == n;
}

AppProduction::AppProduction(MotorHardware& motorHardware, ShiftRegisterBus& spiBus, MqttLink& mqttLink)
  : _motorHardware(motorHardware), _spiBus(spiBus), _mqtt(mqttLink),
    _state(BEAT_IDLE), _beatCmd("load"), _diagBeat(false), _currentBeat(0) {
  memset(_asparagusCounts, 0, sizeof(_asparagusCounts));
  memset(_targetSteps, 0, sizeof(_targetSteps));
  memset(_beatSteps, 0, sizeof(_beatSteps));
  for (int i = 0; i < 8; i++) {
    _carrierState[i] = CARRIER_LOCKED;
    _nextState[i] = CARRIER_LOCKED;
  }
  s_production = this;
}

void AppProduction::setup() {
  LOG_I("--- [生产模式] 启动 ---");
  _state = BEAT_IDLE;
  _beatCmd = "load";
  _diagBeat = false;
  _currentBeat = 0;
  memset(_asparagusCounts, 0, sizeof(_asparagusCounts));
  memset(_targetSteps, 0, sizeof(_targetSteps));
  memset(_beatSteps, 0, sizeof(_beatSteps));
  // 托架状态上电初始化一次：全部锁定（后续 load 不再整体重置）
  for (int i = 0; i < 8; i++) {
    _carrierState[i] = CARRIER_LOCKED;
    _nextState[i] = CARRIER_LOCKED;
  }

  // 注册 MQTT 命令回调
  _mqtt.onCommand([](const char* payload) {
    if (s_production) {
      s_production->handleCommand(payload);
    }
  });

  // 确保所有电机处于静止状态
  _motorHardware.stop();
  _motorHardware.setCurrentPosition(0);

  // 设置为正常工作速度与加速度
  _motorHardware.setMaxSpeed(STEPPER_MAX_SPEED);
  _motorHardware.setAcceleration(STEPPER_ACCELERATION);
}

void AppProduction::stop() {
  LOG_I("--- [生产模式] 结束 ---");
  _motorHardware.stop();
  _mqtt.publishState("idle");
}

void AppProduction::handleCommand(const char* payload) {
  if (_state != BEAT_IDLE) {
    LOG_W("节拍进行中，忽略新命令");
    return;
  }

  String data(payload);
  String cmd;
  if (!jsonGetString(data, "cmd", cmd)) {
    LOG_W("忽略缺少 cmd 字段的命令: %s", payload);
    return;
  }

  if (cmd == "load") {
    if (parseCommand(payload)) {
      executeMove();
    } else {
      LOG_W("忽略无效的 load 命令: %s", payload);
    }
  } else if (cmd == "motor") {
    // 单电机调试命令: {"cmd":"motor","motor":5,"dir":1,"angle":90}
    float motorF = 0, dirF = 0, angle = 0;
    if (!jsonGetFloat(data, "motor", motorF) ||
        !jsonGetFloat(data, "dir", dirF) ||
        !jsonGetFloat(data, "angle", angle)) {
      LOG_W("忽略缺少字段的 motor 命令: %s", payload);
      return;
    }
    long motor = (long)motorF, dir = (long)dirF;
    if (motor < 1 || motor > 8) {
      LOG_W("motor 命令电机号越界 (%ld)，须为 1-8", motor);
      return;
    }
    if (dir != 0 && dir != 1) {
      LOG_W("motor 命令方向非法 (%ld)，0=反转 1=正转", dir);
      return;
    }
    if (angle <= 0.0f || angle > 360.0f) {
      LOG_W("motor 命令角度越界 (%.1f°)，须为 0-360", angle);
      return;
    }
    executeDiagMove((uint8_t)motor, (int)dir, angle);
  } else if (cmd == "multi") {
    // 多电机调试命令 (v1.2): {"cmd":"multi","angles":[90,-45,0,...]}，0=不动作，负值=反转
    float angles[8];
    if (!jsonGetFloatArray(data, "angles", angles, 8)) {
      LOG_W("忽略无效的 multi 命令 (angles 须为 8 个数值): %s", payload);
      return;
    }
    bool valid = true;
    for (int i = 0; i < 8; i++) {
      if (angles[i] < -360.0f || angles[i] > 360.0f) {
        LOG_W("multi 命令 %d 号电机角度越界 (%.1f°)，须为 [-360, 360]", i + 1, angles[i]);
        valid = false;
      }
    }
    if (!valid) {
      return;
    }
    executeMultiMove(angles);
  } else {
    LOG_W("忽略未知命令类型: %s", cmd.c_str());
  }
}

bool AppProduction::parseCommand(const char* payload) {
  // 协议: {"cmd":"load","counts":[n1,...,n8]}，n1 为 1 号托架，n8 为 8 号
  String data(payload);
  float values[8];
  if (!jsonGetFloatArray(data, "counts", values, 8)) {
    return false;
  }

  // 校验：每个值须为 0~255 的整数，越界或带小数则整帧忽略
  for (int i = 0; i < 8; i++) {
    if (values[i] < 0.0f || values[i] > 255.0f || values[i] != (long)values[i]) {
      LOG_W("counts[%d] 非法 (%.2f)，须为 0~255 整数，忽略整帧", i + 1, values[i]);
      return false;
    }
    _asparagusCounts[i] = (uint8_t)values[i];
  }

  LOG_I("接收到芦笋数据: 1号=%d, 2号=%d, 3号=%d, 4号=%d, 5号=%d, 6号=%d, 7号=%d, 8号=%d",
        _asparagusCounts[0], _asparagusCounts[1], _asparagusCounts[2], _asparagusCounts[3],
        _asparagusCounts[4], _asparagusCounts[5], _asparagusCounts[6], _asparagusCounts[7]);
  return true;
}

void AppProduction::planBeats() {
  // 托架状态机规划（doc/节拍状态图.md，配对判定）
  // 供方 k（2~8号，索引1~7）与接收方 k-1（其右侧）成对产生动作：
  //   成对条件 = 接收方处于锁定 且 接收方为空(count=0)
  //              且 接收方的右侧(k-2号)无料（即接收方"可以解锁"）
  //   锁定+count=1 → 美妙+接美妙；锁定+count≥2 → 超载+接超载；转世 → 新生+接新生
  //   不成对则整对不产生：供方保持原状态，双方本拍均不动
  //   1号轮右侧为出料口（虚拟接收位，永远为空、可解锁）：1号轮永远可配对出料
  memset(_beatSteps, 0, sizeof(_beatSteps));

  static const char* kStateNames[] = {
    "锁定", "转世", "美妙", "接美妙", "超载", "接超载", "新生", "接新生"
  };

  // 打印本次 load 判定前的托架当前状态
  LOG_D("load 时状态: 1号=%s, 2号=%s, 3号=%s, 4号=%s, 5号=%s, 6号=%s, 7号=%s, 8号=%s",
        kStateNames[_carrierState[0]], kStateNames[_carrierState[1]],
        kStateNames[_carrierState[2]], kStateNames[_carrierState[3]],
        kStateNames[_carrierState[4]], kStateNames[_carrierState[5]],
        kStateNames[_carrierState[6]], kStateNames[_carrierState[7]]);

  // 判定结果（含配对产生的动作态），默认全部保持原状态
  CarrierState judged[8];
  for (int i = 0; i < 8; i++) {
    judged[i] = _carrierState[i];
    _nextState[i] = _carrierState[i];
  }

  // 逐供方判定（配对各占一个供方+一个接收方，互不重叠，扫描顺序无关）
  for (int i = 0; i < 8; i++) {
    CarrierState st = _carrierState[i];
    int r = i - 1;
    bool donorMaterial = (st == CARRIER_LOCKED && _asparagusCounts[i] >= 1);
    bool donorSamsara = (st == CARRIER_SAMSARA);
    if (!donorMaterial && !donorSamsara) {
      continue;  // 锁定无料：保持锁定
    }

    // 1号轮（i=0）的接收位是出料口，永远可用；其余供方检查真实接收方
    bool canPair = (i == 0) ||
                   ((_carrierState[r] == CARRIER_LOCKED) &&
                    (_asparagusCounts[r] == 0) &&
                    (r == 0 || _asparagusCounts[r - 1] == 0));
    if (!canPair) {
      LOG_D("托架%d: 配对失败（接收方%d不可用），保持%s",
            i + 1, r + 1, kStateNames[st]);
      continue;
    }

    switch (st) {
      case CARRIER_LOCKED:
        if (_asparagusCounts[i] == 1) {
          // 美妙（+接美妙）
          judged[i] = CARRIER_WONDERFUL;
          _beatSteps[0][i] = STEPS_PER_60DEG;  // 拍1 旋转60°
          _beatSteps[2][i] = STEPS_PER_30DEG;  // 拍3 旋转30°
          _nextState[i] = CARRIER_LOCKED;
          if (i > 0) {
            judged[r] = CARRIER_ACCEPT_WONDERFUL;
            _beatSteps[0][r] = STEPS_PER_30DEG;  // 拍1 旋转30°
            _beatSteps[1][r] = STEPS_PER_60DEG;  // 拍2 旋转60°
            _nextState[r] = CARRIER_LOCKED;
          }
        } else {
          // 超载（+接超载）
          judged[i] = CARRIER_OVERLOAD;
          _beatSteps[0][i] = STEPS_PER_28DEG;  // 拍1 旋转28°
          _nextState[i] = CARRIER_SAMSARA;     // → 转世
          if (i > 0) {
            judged[r] = CARRIER_ACCEPT_OVERLOAD;
            _beatSteps[1][r] = STEPS_PER_90DEG;  // 拍2 旋转90°
            _nextState[r] = CARRIER_LOCKED;
          }
        }
        break;
      case CARRIER_SAMSARA:
        // 新生（+接新生）
        judged[i] = CARRIER_NEWBORN;
        _beatSteps[0][i] = STEPS_PER_32DEG;    // 拍1 旋转32°
        _beatSteps[2][i] = STEPS_PER_30DEG;    // 拍3 旋转30°
        _nextState[i] = CARRIER_LOCKED;
        if (i > 0) {
          judged[r] = CARRIER_ACCEPT_NEWBORN;
          _beatSteps[0][r] = STEPS_PER_30DEG;  // 拍1 旋转30°
          _beatSteps[1][r] = STEPS_PER_60DEG;  // 拍2 旋转60°
          _nextState[r] = CARRIER_LOCKED;
        }
        break;
      default:
        break;
    }
  }

  for (int i = 0; i < 8; i++) {
    LOG_D("托架%d: 数量=%d → %s",
          i + 1, _asparagusCounts[i], kStateNames[judged[i]]);
  }

  for (int b = 0; b < 3; b++) {
    LOG_D("节拍%d 步数: M0=%ld, M1=%ld, M2=%ld, M3=%ld, M4=%ld, M5=%ld, M6=%ld, M7=%ld",
          b + 1, _beatSteps[b][0], _beatSteps[b][1], _beatSteps[b][2], _beatSteps[b][3],
          _beatSteps[b][4], _beatSteps[b][5], _beatSteps[b][6], _beatSteps[b][7]);
  }
}

void AppProduction::startBeat(uint8_t beat) {
  // 全体电机逻辑正转（输送方向），经 74HC595 一次写入方向数据
  //（0xFF 经 DIR_INVERT_MASK 换算为物理方向），随后 8 路同时启动。
  // 启动循环内不得插入 LOG_I（阻塞式 MQTT 发送会把各路启动时刻拉开 ~10ms）
  _spiBus.transfer(0xFF ^ DIR_INVERT_MASK);
  for (int i = 0; i < 8; i++) {
    if (_beatSteps[beat][i] > 0) {
      _motorHardware.startMove(i, _beatSteps[beat][i]);
    }
  }
}

void AppProduction::applyBeatTransitions(uint8_t beat) {
  // 新状态机无拍间转移；三拍结束（拍3完成或被跳过）时统一落位整备C终态
  if (beat != 2) {
    return;
  }
  for (int i = 0; i < 8; i++) {
    _carrierState[i] = _nextState[i];
  }
}

void AppProduction::executeMove() {
  planBeats();

  _beatCmd = "load";
  _currentBeat = 0;

  // 三拍全 0（如全部上岗）：无需动作，直接完成
  bool anyMove = false;
  for (int b = 0; b < 3 && !anyMove; b++) {
    for (int i = 0; i < 8; i++) {
      if (_beatSteps[b][i] > 0) {
        anyMove = true;
        break;
      }
    }
  }

  if (!anyMove) {
    LOG_I("当前节拍没有电机需要旋转。");
    applyBeatTransitions(2);  // 无动作仍需落位整备C终态
    _state = BEAT_COMPLETED;
    return;
  }

  _mqtt.publishState("running");
  _state = BEAT_RUNNING;
  startBeat(0);
}

void AppProduction::executeDiagMove(uint8_t motor1to8, int dir, float angleDeg) {
  uint8_t idx = motor1to8 - 1; // 数组索引 0-7

  // 角度 → 步数 (1/16 细分、3:1 减速下 90° = 2400 步)，支持 22.5 等小数角度
  long steps = (long)(angleDeg * (float)STEPS_PER_90DEG / 90.0f + 0.5f);

  LOG_I("调试运动: 电机 %d 号 %s %.1f° (%ld 步)",
        motor1to8, dir ? "正转" : "反转", angleDeg, steps);

  // 调试运动采用低速参数，节拍完成后恢复
  _motorHardware.setMaxSpeed(STEPPER_DIAG_SPEED);
  _motorHardware.setAcceleration(STEPPER_DIAG_ACCEL);

  // 方向经 74HC595 输出: 逻辑正转该电机位为 1，反转为 0，
  // 再经 DIR_INVERT_MASK 换算为物理方向
  _spiBus.transfer((dir ? (1 << idx) : 0) ^ DIR_INVERT_MASK);

  _motorHardware.startMove(idx, steps);

  _diagBeat = true;
  _beatCmd = "motor";
  _mqtt.publishState("running");
  _state = BEAT_RUNNING;
}

void AppProduction::executeMultiMove(const float angles[8]) {
  // 调试运动采用低速参数，节拍完成后恢复
  _motorHardware.setMaxSpeed(STEPPER_DIAG_SPEED);
  _motorHardware.setAcceleration(STEPPER_DIAG_ACCEL);

  uint8_t dirBits = 0;
  bool anyMove = false;
  for (int i = 0; i < 8; i++) {
    // 角度 → 步数 (1/16 细分下 90° = 800 步)，绝对值换算，符号决定方向
    float a = angles[i];
    _targetSteps[i] = (long)(fabsf(a) * (float)STEPS_PER_90DEG / 90.0f + 0.5f);
    if (_targetSteps[i] > 0) {
      if (a > 0) {
        dirBits |= (uint8_t)(1 << i); // 正转该电机位为 1，反转为 0
      }
      anyMove = true;
    }
  }

  LOG_I("多电机调试: [%.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f]°",
        angles[0], angles[1], angles[2], angles[3],
        angles[4], angles[5], angles[6], angles[7]);
  LOG_D("multi 步数: M0=%ld, M1=%ld, M2=%ld, M3=%ld, M4=%ld, M5=%ld, M6=%ld, M7=%ld",
        _targetSteps[0], _targetSteps[1], _targetSteps[2], _targetSteps[3],
        _targetSteps[4], _targetSteps[5], _targetSteps[6], _targetSteps[7]);

  // 方向数据一次写入 74HC595（经 DIR_INVERT_MASK 换算物理方向），随后 8 路电机同时启动。
  // 启动循环内不得插入 LOG_I（阻塞式 MQTT 发送会把各路启动时刻拉开 ~10ms），
  // 先记录启动时差、全部启动后再统一输出
  _spiBus.transfer(dirBits ^ DIR_INVERT_MASK);
  uint32_t t0 = micros();
  uint32_t dtStart[NUM_MOTORS];
  for (int i = 0; i < NUM_MOTORS; i++) {
    dtStart[i] = 0;
    if (_targetSteps[i] > 0) {
      dtStart[i] = micros() - t0;
      _motorHardware.startMove(i, _targetSteps[i]);
    }
  }
  LOG_I("multi 启动时刻 t=%lu us", t0);
  for (int i = 0; i < NUM_MOTORS; i++) {
    if (_targetSteps[i] > 0) {
      LOG_I("  M%d startMove dt=%lu us", i, dtStart[i]);
    }
  }

  _diagBeat = true;
  _beatCmd = "multi";
  if (!anyMove) {
    LOG_I("multi: 无电机需要动作。");
    _state = BEAT_COMPLETED;
    return;
  }
  _mqtt.publishState("running");
  _state = BEAT_RUNNING;
}

void AppProduction::loop() {
  switch (_state) {
    case BEAT_IDLE:
      // 空闲等待 MQTT 命令 (handleCommand 触发状态迁移)
      break;

    case BEAT_RUNNING: {
      // 等待当前节拍内全部电机到位
      if (_motorHardware.isMoving()) {
        break;
      }
      LOG_I("节拍%d 完成", _currentBeat + 1);

      // 拍间托架状态转移
      applyBeatTransitions(_currentBeat);
      _currentBeat++;

      // 启动下一节拍；全零节拍直接跳过（仍执行拍间转移）
      bool started = false;
      while (_currentBeat < 3) {
        long total = 0;
        for (int i = 0; i < 8; i++) {
          total += _beatSteps[_currentBeat][i];
        }
        if (total > 0) {
          startBeat(_currentBeat);
          started = true;
          break;
        }
        applyBeatTransitions(_currentBeat);
        _currentBeat++;
      }

      if (!started) {
        _state = BEAT_COMPLETED;
      }
      break;
    }

    case BEAT_COMPLETED: {
      // 动作结束，停止所有电机
      _motorHardware.stop();

      // 调试节拍结束后恢复生产速度参数
      if (_diagBeat) {
        _diagBeat = false;
        _motorHardware.setMaxSpeed(STEPPER_MAX_SPEED);
        _motorHardware.setAcceleration(STEPPER_ACCELERATION);
      }

      // 发布节拍完成应答（回带命令类型）
      _mqtt.publishDone(_beatCmd);
      _mqtt.publishState("idle");
      LOG_I("节拍完成(%s)，已发布 done 应答", _beatCmd);

      // 重置，进入下一个节拍等待
      _state = BEAT_IDLE;
      break;
    }
  }
}
