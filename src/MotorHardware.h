#ifndef MOTOR_HARDWARE_H
#define MOTOR_HARDWARE_H

#include <Arduino.h>
#include "FastAccelStepper.h"
#include "config.h"

// 8 路独立步进电机硬件层。
// 驱动芯片使能脚 (EN) 直接接地常使能，无软件使能概念；
// 电机是否运动仅取决于是否向其下发脉冲；真实方向经 74HC595 输出。
class MotorHardware {
public:
  MotorHardware(const uint8_t* stepPins);

  void begin(float maxSpeed, float acceleration);
  void setMaxSpeed(float speed);
  void setAcceleration(float accel);

  // 单电机运动控制 (motorIndex: 0-7)
  void startMove(uint8_t motorIndex, long steps);

  // 停止所有电机
  void stop();

  // 是否有任一电机在运动
  bool isMoving() const;

  // 所有电机位置清零
  void setCurrentPosition(long pos);

private:
  FastAccelStepper* _steppers[NUM_MOTORS];
  uint8_t _stepPins[NUM_MOTORS];
};

#endif // MOTOR_HARDWARE_H
