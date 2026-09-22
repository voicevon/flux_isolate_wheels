#include "AppMotorDiag.h"
#include "MotorHardware.h"
#include "ShiftRegisterBus.h"
#include "Logger.h"
#include <Arduino.h>

AppMotorDiag::AppMotorDiag(MotorHardware& motorHardware, ShiftRegisterBus& spiBus) 
  : _motorHardware(motorHardware), _spiBus(spiBus), _currentMotor(0), _testState(TEST_IDLE), _pauseTimer(0) {}

void AppMotorDiag::setup() {
  LOG_I("--- [诊断模式] 电机诊断启动 ---");
  LOG_I("短按 GPIO 0 按键可在 M0-M7 电机间循环切换");
  
  _currentMotor = 0;
  _testState = TEST_IDLE;
  _pauseTimer = 0;
  
  // 设置诊断专用的低速度和加速度
  _motorHardware.setMaxSpeed(STEPPER_DIAG_SPEED);
  _motorHardware.setAcceleration(STEPPER_DIAG_ACCEL);
  
  // 复位位置，确保静止
  _motorHardware.stop();
  _motorHardware.setCurrentPosition(0);
}

void AppMotorDiag::stop() {
  LOG_I("--- [诊断模式] 电机诊断结束 ---");

  // 恢复正常的生产速度和加速度配置
  _motorHardware.setMaxSpeed(STEPPER_MAX_SPEED);
  _motorHardware.setAcceleration(STEPPER_ACCELERATION);

  _motorHardware.stop();
}

void AppMotorDiag::nextMotor() {
  _currentMotor = (_currentMotor + 1) % NUM_MOTORS;
  LOG_I("[诊断模式] 切换至测试电机 M%d", _currentMotor);
  
  // 停止当前动作并复位状态
  _motorHardware.stop();
  _motorHardware.setCurrentPosition(0);
  _testState = TEST_IDLE;
}

void AppMotorDiag::loop() {
  switch (_testState) {
    case TEST_IDLE:
      _testState = TEST_MOVE_RIGHT;
      break;

    case TEST_MOVE_RIGHT:
      LOG_D("[诊断模式] 电机 M%d 向右旋转 %.2f 圈", _currentMotor, DIAG_TARGET_ROTATIONS);
      // 设置方向：当前测试电机逻辑正转（位为 1），经掩码换算物理方向
      _spiBus.transfer((1 << _currentMotor) ^ DIR_INVERT_MASK);
      _motorHardware.startMove(_currentMotor, DIAG_STEPS);
      _testState = TEST_WAIT_RIGHT;
      break;

    case TEST_WAIT_RIGHT:
      if (!_motorHardware.isMoving()) {
        _motorHardware.setCurrentPosition(0);
        _pauseTimer = millis();
        _testState = TEST_PAUSE_BEFORE_LEFT;
      }
      break;

    case TEST_PAUSE_BEFORE_LEFT:
      if (millis() - _pauseTimer >= 500) {
        _testState = TEST_MOVE_LEFT;
      }
      break;

    case TEST_MOVE_LEFT:
      LOG_D("[诊断模式] 电机 M%d 向左旋转 %.2f 圈", _currentMotor, DIAG_TARGET_ROTATIONS);
      // 设置方向：所有位为 0 (逻辑反转)，经掩码换算物理方向
      _spiBus.transfer(0 ^ DIR_INVERT_MASK);
      _motorHardware.startMove(_currentMotor, DIAG_STEPS);
      _testState = TEST_WAIT_LEFT;
      break;

    case TEST_WAIT_LEFT:
      if (!_motorHardware.isMoving()) {
        _motorHardware.setCurrentPosition(0);
        _pauseTimer = millis();
        _testState = TEST_PAUSE_BEFORE_RIGHT;
      }
      break;

    case TEST_PAUSE_BEFORE_RIGHT:
      if (millis() - _pauseTimer >= 500) {
        _testState = TEST_MOVE_RIGHT;
      }
      break;
    
    default:
      break;
  }
}
