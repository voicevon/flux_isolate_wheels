#ifndef APP_MOTOR_DIAG_H
#define APP_MOTOR_DIAG_H

#include "AppBase.h"
#include "config.h"

// --- 电机诊断配置 ---
#define DIAG_TARGET_ROTATIONS 0.25f   // 诊断模式下执行机构目标旋转圈数
#define DIAG_STEPS  ((long)(MOTOR_FULL_STEPS * MICROSTEP_RESOLUTION * GEAR_RATIO * DIAG_TARGET_ROTATIONS))

class MotorHardware;
class ShiftRegisterBus;

class AppMotorDiag : public AppBase {
public:
  AppMotorDiag(MotorHardware& motorHardware, ShiftRegisterBus& spiBus);
  void setup() override;
  void loop() override;
  void stop() override;

  // 短按时触发：切换到下一个电机测试
  void nextMotor();

private:
  MotorHardware& _motorHardware;
  ShiftRegisterBus& _spiBus;
  int _currentMotor;

  enum TestState {
    TEST_IDLE,
    TEST_MOVE_RIGHT,
    TEST_WAIT_RIGHT,
    TEST_PAUSE_BEFORE_LEFT,
    TEST_MOVE_LEFT,
    TEST_WAIT_LEFT,
    TEST_PAUSE_BEFORE_RIGHT
  };

  TestState _testState;
  unsigned long _pauseTimer;
};

#endif // APP_MOTOR_DIAG_H
