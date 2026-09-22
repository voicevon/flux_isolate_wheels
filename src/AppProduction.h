#ifndef APP_PRODUCTION_H
#define APP_PRODUCTION_H

#include "AppBase.h"
#include <stdint.h>

class MotorHardware;
class ShiftRegisterBus;
class MqttLink;

class AppProduction : public AppBase {
public:
  AppProduction(MotorHardware& motorHardware, ShiftRegisterBus& spiBus, MqttLink& mqttLink);
  void setup() override;
  void loop() override;
  void stop() override;

  // MQTT 命令回调入口（由 MqttLink 订阅分发触发）
  void handleCommand(const char* payload);

private:
  // 解析 JSON 命令: {"cmd":"load","counts":[n1,...,n8]}
  bool parseCommand(const char* payload);
  // 按业务规则规划各转轮步数并下发运动
  void executeMove();

  MotorHardware& _motorHardware;
  ShiftRegisterBus& _spiBus;
  MqttLink& _mqtt;

  // 1-8号托架转轮上的芦笋数量状态（对应索引 0-7，其中 0对应1号，7对应8号）
  uint8_t _asparagusCounts[8];

  enum BeatState {
    BEAT_IDLE,
    BEAT_RUNNING,
    BEAT_COMPLETED
  };

  BeatState _state;

  // 电机在当前节拍的目标步数
  long _targetSteps[8];
};

#endif // APP_PRODUCTION_H
