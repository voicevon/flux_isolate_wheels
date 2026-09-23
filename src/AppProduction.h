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
  // 按托架状态机（doc/节拍逻辑.md 第4节）规划三节拍各电机步数并更新托架状态
  void planBeats();
  // 启动指定节拍：写方向数据后 8 路电机同时启动（启动循环内不得插入日志）
  void startBeat(uint8_t beat);
  // 节拍完成后的托架状态转移（轮回→锁定/自由、接客/美妙→自由）
  void applyBeatTransitions(uint8_t beat);
  // 按业务规则规划各转轮步数并下发运动（load 命令入口）
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

  // 托架状态机状态（doc/节拍逻辑.md 第4.1节），上电时初始化为自由（仅一次）
  enum CarrierState {
    CARRIER_FREE,       // 自由
    CARRIER_ON_DUTY,    // 上岗
    CARRIER_GUEST,      // 接客
    CARRIER_WONDERFUL,  // 美妙
    CARRIER_OVERLOAD,   // 超载
    CARRIER_LOCKED,     // 锁定
    CARRIER_SAMSARA     // 轮回
  };
  CarrierState _carrierState[8];

  // 三节拍各电机的步数增量（[节拍][电机索引]），由 planBeats 生成
  long _beatSteps[3][8];
  // 当前执行的节拍索引 0-2
  uint8_t _currentBeat;

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
