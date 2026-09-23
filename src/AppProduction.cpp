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
    _carrierState[i] = CARRIER_FREE;
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
  // 托架状态上电初始化一次：全部自由（后续 load 不再整体重置）
  for (int i = 0; i < 8; i++) {
    _carrierState[i] = CARRIER_FREE;
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
  // 托架状态机规划（doc/节拍逻辑.md 第4节）
  // 托架编号 1-8 对应索引 0-7；右邻 = 编号减一 (i-1)，左邻 = 编号加一 (i+1)
  memset(_beatSteps, 0, sizeof(_beatSteps));

  static const char* kStateNames[] = {
    "自由", "上岗", "接客", "美妙", "超载", "锁定", "轮回"
  };

  // ⑥ 上游锁定（不递归）：记录本次判定中由 ②/③ 产生的锁定，
  // ⑥ 新产生的锁定不置位，避免向更上游传导
  bool primaryLocked[8] = { false, false, false, false, false, false, false, false };

  // 打印本次 load 判定前的托架当前状态
  LOG_D("load 时状态: 1号=%s, 2号=%s, 3号=%s, 4号=%s, 5号=%s, 6号=%s, 7号=%s, 8号=%s",
        kStateNames[_carrierState[0]], kStateNames[_carrierState[1]],
        kStateNames[_carrierState[2]], kStateNames[_carrierState[3]],
        kStateNames[_carrierState[4]], kStateNames[_carrierState[5]],
        kStateNames[_carrierState[6]], kStateNames[_carrierState[7]]);

  for (int i = 0; i < 8; i++) {
    uint8_t selfCount = _asparagusCounts[i];
    uint8_t rightCount = (i > 0) ? _asparagusCounts[i - 1] : 0;
    uint8_t leftCount = (i < 7) ? _asparagusCounts[i + 1] : 0;
    CarrierState st = _carrierState[i];

    // 超载/轮回不参与重新判定（超载当拍即转轮回；轮回在节拍1完成后按 ⑤ 处理）
    if (st == CARRIER_OVERLOAD || st == CARRIER_SAMSARA) {
      continue;
    }

    // 锁定无条件回自由（下次 load），随后按 ① 重新判定
    if (st == CARRIER_LOCKED) {
      st = CARRIER_FREE;
    }

    bool lockedBy23 = false;  // 本次判定是否被 ②/③ 锁定

    // ① 按自身芦笋数量（自由/上岗均参与判定：上岗轮传感器显示有料时
    // 同样转美妙/超载，避免带料的上岗轮永远不出料）
    if (st == CARRIER_FREE || st == CARRIER_ON_DUTY) {
      if (selfCount == 0) {
        st = CARRIER_ON_DUTY;
      } else if (selfCount == 1) {
        st = CARRIER_WONDERFUL;
      } else {
        st = CARRIER_OVERLOAD;
      }
    }

    // ② 上岗：先判锁定（右邻>0，优先），再判接客（右邻=0 且 左邻>0）
    if (st == CARRIER_ON_DUTY) {
      if (rightCount > 0) {
        st = CARRIER_LOCKED;
        lockedBy23 = true;
      } else if (leftCount > 0) {
        st = CARRIER_GUEST;
      }
    }

    // ③ 右邻有料：美妙/接客/超载统一覆盖为锁定
    if ((st == CARRIER_WONDERFUL || st == CARRIER_GUEST || st == CARRIER_OVERLOAD) &&
        rightCount > 0) {
      st = CARRIER_LOCKED;
      lockedBy23 = true;
    }

    // ④ 超载无条件转轮回
    if (st == CARRIER_OVERLOAD) {
      st = CARRIER_SAMSARA;
    }

    // ⑥ 上游锁定（不递归）：右邻被 ②/③ 锁定（被锁定的空轮）时，
    // 有料托架本拍不得投料，避免把物料投到不动的轮子上；
    // ⑥ 产生的锁定不置 primaryLocked，不向更上游传导
    if ((st == CARRIER_WONDERFUL || st == CARRIER_SAMSARA) &&
        i > 0 && primaryLocked[i - 1]) {
      st = CARRIER_LOCKED;
      LOG_D("托架%d: ⑥ 右邻被锁定，本拍不投料", i + 1);
    }

    _carrierState[i] = st;
    primaryLocked[i] = lockedBy23;

    // 节拍动作（表中角度为绝对位置，换算为各拍增量步数）
    switch (st) {
      case CARRIER_GUEST:
        _beatSteps[0][i] = STEPS_PER_30DEG;  // 拍1 → 绝对 30°
        _beatSteps[1][i] = STEPS_PER_60DEG;  // 拍2 30°→90°（到位）
        break;
      case CARRIER_WONDERFUL:
        _beatSteps[0][i] = STEPS_PER_60DEG;  // 拍1 → 绝对 60°
        _beatSteps[2][i] = STEPS_PER_30DEG;  // 拍3 60°→90°（到位）
        break;
      case CARRIER_SAMSARA:
        _beatSteps[0][i] = STEPS_PER_60DEG;  // 拍1 → 绝对 60°
        break;
      default:
        break;  // 锁定/上岗不动作
    }

    LOG_D("托架%d: 数量=%d 右邻=%d 左邻=%d → %s",
          i + 1, selfCount, rightCount, leftCount, kStateNames[st]);
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
  for (int i = 0; i < 8; i++) {
    uint8_t rightCount = (i > 0) ? _asparagusCounts[i - 1] : 0;
    switch (beat) {
      case 0:  // 节拍1完成：轮回按 ⑤ 判定（右邻有料→锁定，否则→自由）
        if (_carrierState[i] == CARRIER_SAMSARA) {
          _carrierState[i] = (rightCount > 0) ? CARRIER_LOCKED : CARRIER_FREE;
        }
        break;
      case 1:  // 节拍2完成：接客到位 → 自由
        if (_carrierState[i] == CARRIER_GUEST) {
          _carrierState[i] = CARRIER_FREE;
        }
        break;
      case 2:  // 节拍3完成：美妙到位 → 自由
        if (_carrierState[i] == CARRIER_WONDERFUL) {
          _carrierState[i] = CARRIER_FREE;
        }
        break;
    }
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
