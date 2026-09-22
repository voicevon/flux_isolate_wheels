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
  // 解析生产节拍命令的 counts: {"cmd":"load","counts":[n1,...,n8]}
  bool parseCommand(const char* payload);
  // 按业务规则规划各转轮步数并下发运动
  void executeMove();
  // 单电机调试运动（motor1to8: 托架号 1-8, dir: 1正转/0反转, angleDeg: 角度）
  void executeDiagMove(uint8_t motor1to8, int dir, float angleDeg);
  // 多电机调试运动（v1.2）: angles[8] 依次为 1~8 号电机角度，0=不动作，负值=反转
  void executeMultiMove(const float angles[8]);

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

  // 当前节拍类型: "load" 生产节拍 / "motor" 单机调试（用于 done 应答回带）
  const char* _beatCmd;
  // 单机调试节拍标志（完成时需恢复生产速度参数）
  bool _diagBeat;
};

#endif // APP_PRODUCTION_H
