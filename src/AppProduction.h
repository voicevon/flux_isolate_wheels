#ifndef APP_PRODUCTION_H
#define APP_PRODUCTION_H

#include "AppBase.h"
#include <BluetoothSerial.h>
#include <stdint.h>

class MotorHardware;
class ShiftRegisterBus;

class AppProduction : public AppBase {
public:
  AppProduction(MotorHardware& motorHardware, ShiftRegisterBus& spiBus);
  void setup() override;
  void loop() override;
  void stop() override;

private:
  // 解析蓝牙数据
  bool parseBluetoothData(const String& data);
  // 执行两阶段同步脉冲控制
  void executeTwoPhaseMove();

  MotorHardware& _motorHardware;
  ShiftRegisterBus& _spiBus;
  BluetoothSerial _btSerial;

  // 1-8号托架转轮上的芦笋数量状态（对应索引 0-7，其中 0对应1号，7对应8号）
  uint8_t _asparagusCounts[8];

  enum BeatState {
    BEAT_WAIT_DATA,
    BEAT_PHASE_1_RUN,
    BEAT_PHASE_2_RUN,
    BEAT_COMPLETED
  };

  BeatState _state;
  unsigned long _lastTimeoutPrint;

  // 电机在当前节拍的目标步数
  long _targetSteps[8];
};

#endif // APP_PRODUCTION_H
