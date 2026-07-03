#ifndef MOTOR_HARDWARE_H
#define MOTOR_HARDWARE_H

#include <Arduino.h>
#include "FastAccelStepper.h"

class MotorHardware {
public:
  MotorHardware(const uint8_t* enablePins, uint8_t numMotors);
  ~MotorHardware();

  void begin(float maxSpeed, float acceleration);
  void setMaxSpeed(float speed);
  void setAcceleration(float accel);
  
  // 设置全体电机使能屏蔽：mask 中位 i 为 1 表示电机 i 启用（放行，引脚 HIGH），0 表示屏蔽（引脚 LOW）
  void setEnableMask(uint8_t enableMask);
  
  // 单一电机设置使能状态 (true = 放行/HIGH, false = 屏蔽/LOW)
  void setMotorEnabled(uint8_t motorIndex, bool enabled);

  // 步进控制代理方法
  void startMove(long steps);
  void stop();
  bool isMoving() const;
  void setCurrentPosition(long pos);

private:
  FastAccelStepper* _stepper;
  uint8_t* _enablePins;
  uint8_t _numMotors;
};

#endif // MOTOR_HARDWARE_H
