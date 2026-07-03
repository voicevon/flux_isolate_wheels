#include "AppProduction.h"
#include "MotorHardware.h"
#include "ShiftRegisterBus.h"
#include "config.h"
#include "pins.h"
#include "Logger.h"
#include <Arduino.h>

AppProduction::AppProduction(MotorHardware& motorHardware, ShiftRegisterBus& spiBus)
  : _motorHardware(motorHardware), _spiBus(spiBus), _state(BEAT_WAIT_DATA), _lastTimeoutPrint(0) {
  memset(_asparagusCounts, 0, sizeof(_asparagusCounts));
  memset(_targetSteps, 0, sizeof(_targetSteps));
}

void AppProduction::setup() {
  LOG_I("--- [生产模式] 启动 ---");
  _state = BEAT_WAIT_DATA;
  _lastTimeoutPrint = 0;
  memset(_asparagusCounts, 0, sizeof(_asparagusCounts));
  memset(_targetSteps, 0, sizeof(_targetSteps));

  // 蓝牙初始化
  if (!_btSerial.begin("FluxLoader_BT")) {
    LOG_E("蓝牙初始化失败！");
  } else {
    LOG_I("蓝牙已就绪，配对名称: FluxLoader_BT");
  }

  // 确保所有电机处于静止且被屏蔽状态
  _motorHardware.setEnableMask(0x00);
  _motorHardware.stop();
  _motorHardware.setCurrentPosition(0);

  // 设置为正常工作速度与加速度
  _motorHardware.setMaxSpeed(STEPPER_MAX_SPEED);
  _motorHardware.setAcceleration(STEPPER_ACCELERATION);
}

void AppProduction::stop() {
  LOG_I("--- [生产模式] 结束 ---");
  _btSerial.end();
  _motorHardware.stop();
  _motorHardware.setEnableMask(0x00);
}

bool AppProduction::parseBluetoothData(const String& data) {
  // 协议格式: $LOADER,n1,n2,n3,n4,n5,n6,n7,n8
  // 数据代表 1号到8号 托架上的芦笋数量 (n1是1号, n8是8号)
  if (!data.startsWith("$LOADER,")) {
    return false;
  }

  // 提取各个转轮的数据
  int commaIndex = 7; // '$LOADER,' 的末尾
  for (int i = 0; i < 8; i++) {
    int nextComma = data.indexOf(',', commaIndex + 1);
    if (nextComma == -1 && i < 7) {
      return false; // 逗号数量不足
    }
    String valStr = (i == 7) ? data.substring(commaIndex + 1) : data.substring(commaIndex + 1, nextComma);
    valStr.trim();
    _asparagusCounts[i] = valStr.toInt();
    commaIndex = nextComma;
  }

  LOG_I("接收到芦笋数据: 1号=%d, 2号=%d, 3号=%d, 4号=%d, 5号=%d, 6号=%d, 7号=%d, 8号=%d",
        _asparagusCounts[0], _asparagusCounts[1], _asparagusCounts[2], _asparagusCounts[3],
        _asparagusCounts[4], _asparagusCounts[5], _asparagusCounts[6], _asparagusCounts[7]);
  return true;
}

void AppProduction::executeTwoPhaseMove() {
  // 根据业务规则判定各转轮的动作步数：
  // 托架编号 1-8，对应数组索引 0-7

  // 1. 1号转轮固定旋转 90° (索引 0)
  _targetSteps[0] = STEPS_PER_90DEG;

  // 2. 2号至8号转轮 (索引 i = 1 至 7)
  for (int i = 1; i < 8; i++) {
    uint8_t rightNeighborCount = _asparagusCounts[i - 1]; // 它的右侧相邻是 i-1 号
    uint8_t selfCount = _asparagusCounts[i];

    if (rightNeighborCount >= 1) {
      // 右侧有芦笋，停止不动作
      _targetSteps[i] = 0;
    } else {
      // 右侧无芦笋，看自身状态
      if (selfCount == 0) {
        // 自身也为空，旋转 90°
        _targetSteps[i] = STEPS_PER_90DEG;
      } else if (selfCount == 1) {
        // 自身有 1 个物料，旋转 90°
        _targetSteps[i] = STEPS_PER_90DEG;
      } else {
        // 自身有多个物料 (>= 2)，旋转 22.5°
        _targetSteps[i] = STEPS_PER_22_5DEG;
      }
    }
  }

  // 打印本次节拍的运动步数规划
  LOG_D("节拍运动步数: M0=%ld, M1=%ld, M2=%ld, M3=%ld, M4=%ld, M5=%ld, M6=%ld, M7=%ld",
        _targetSteps[0], _targetSteps[1], _targetSteps[2], _targetSteps[3],
        _targetSteps[4], _targetSteps[5], _targetSteps[6], _targetSteps[7]);

  // --- 两阶段脉冲控制 ---
  
  // 第一阶段：所有需要旋转的电机 (无论 90° 还是 22.5°) 均参与
  uint8_t phase1Mask = 0;
  for (int i = 0; i < 8; i++) {
    if (_targetSteps[i] > 0) {
      phase1Mask |= (1 << i);
    }
  }

  if (phase1Mask == 0) {
    LOG_I("当前节拍没有电机需要旋转。");
    _state = BEAT_COMPLETED;
    return;
  }

  // 开始第一阶段：走 22.5°
  _motorHardware.stop();
  _motorHardware.setCurrentPosition(0);
  _spiBus.transfer(0xFF); // 全体电机正向旋转，HC595 方向数据设为 1
  _motorHardware.setEnableMask(phase1Mask);
  
  // 以 22.5° 的脉冲数作为目标
  _motorHardware.startMove(STEPS_PER_22_5DEG);
  _state = BEAT_PHASE_1_RUN;
}

void AppProduction::loop() {
  switch (_state) {
    case BEAT_WAIT_DATA: {
      if (_btSerial.available()) {
        String data = _btSerial.readStringUntil('\n');
        data.trim();
        if (parseBluetoothData(data)) {
          executeTwoPhaseMove();
        } else {
          LOG_W("忽略无效的蓝牙数据格式: %s", data.c_str());
        }
      } else {
        if (millis() - _lastTimeoutPrint >= 5000) {
          _lastTimeoutPrint = millis();
          LOG_I("等待手机蓝牙数据...");
        }
      }
      break;
    }

    case BEAT_PHASE_1_RUN: {
      if (!_motorHardware.isMoving()) {
        // 第一阶段（22.5°）运动结束，开始第二阶段（补充脉冲）
        // 只有目标是 90° 的电机需要继续使能
        uint8_t phase2Mask = 0;
        for (int i = 0; i < 8; i++) {
          if (_targetSteps[i] == STEPS_PER_90DEG) {
            phase2Mask |= (1 << i);
          }
        }

        if (phase2Mask != 0) {
          _motorHardware.setCurrentPosition(0);
          _motorHardware.setEnableMask(phase2Mask);
          // 补充 90 - 22.5 = 67.5° 所需步数
          long remainSteps = STEPS_PER_90DEG - STEPS_PER_22_5DEG;
          _motorHardware.startMove(remainSteps);
          _state = BEAT_PHASE_2_RUN;
        } else {
          // 没有需要旋转 90° 的电机，节拍已完成
          _state = BEAT_COMPLETED;
        }
      }
      break;
    }

    case BEAT_PHASE_2_RUN: {
      if (!_motorHardware.isMoving()) {
        _state = BEAT_COMPLETED;
      }
      break;
    }

    case BEAT_COMPLETED: {
      // 动作结束，关闭所有使能
      _motorHardware.setEnableMask(0x00);
      _motorHardware.stop();

      // 向手机发送节拍完成应答
      _btSerial.println("$DONE");
      LOG_I("节拍完成，发送 $DONE 给手机");

      // 重置，进入下一个节拍等待
      _state = BEAT_WAIT_DATA;
      break;
    }
  }
}
