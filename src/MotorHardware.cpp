#include "MotorHardware.h"
#include "pins.h"

// 全局静态的 FastAccelStepperEngine 实例
static FastAccelStepperEngine engine = FastAccelStepperEngine();

MotorHardware::MotorHardware(const uint8_t* stepPins) {
  for (int i = 0; i < NUM_MOTORS; i++) {
    _stepPins[i] = stepPins[i];
    _steppers[i] = NULL;
  }
}

void MotorHardware::begin(float maxSpeed, float acceleration) {
  engine.init();

  // 8 路电机各接独立 STEP 引脚，DIR 引脚为虚拟脚（真实方向经 74HC595 输出）
  // 显式指定 RMT 后端：MCPWM/PCNT 后端（库默认优先分配给前 6 路）为实验性实现，
  // 实测多电机并发时出现轮流动作现象，RMT 每路独立硬件通道可保证 8 路严格并行
  for (int i = 0; i < NUM_MOTORS; i++) {
    _steppers[i] = engine.stepperConnectToPin(_stepPins[i], FasDriver::RMT);
    if (_steppers[i]) {
      _steppers[i]->setDirectionPin(DUMMY_DIR_PIN);
      _steppers[i]->setSpeedInHz(maxSpeed);
      _steppers[i]->setAcceleration(acceleration);
    } else {
      Serial.printf("MotorHardware: 电机 M%d (GPIO %d) 步进队列分配失败\r\n",
                    i, _stepPins[i]);
    }
  }
}

void MotorHardware::setMaxSpeed(float speed) {
  for (int i = 0; i < NUM_MOTORS; i++) {
    if (_steppers[i]) {
      _steppers[i]->setSpeedInHz(speed);
    }
  }
}

void MotorHardware::setAcceleration(float accel) {
  for (int i = 0; i < NUM_MOTORS; i++) {
    if (_steppers[i]) {
      _steppers[i]->setAcceleration(accel);
    }
  }
}

void MotorHardware::startMove(uint8_t motorIndex, long steps) {
  if (motorIndex < NUM_MOTORS && _steppers[motorIndex]) {
    _steppers[motorIndex]->move(steps);
  }
}

void MotorHardware::stop() {
  for (int i = 0; i < NUM_MOTORS; i++) {
    if (_steppers[i]) {
      _steppers[i]->stopMove();
    }
  }
}

bool MotorHardware::isMoving() const {
  for (int i = 0; i < NUM_MOTORS; i++) {
    if (_steppers[i] && _steppers[i]->isRunning()) {
      return true;
    }
  }
  return false;
}

bool MotorHardware::isMotorRunning(uint8_t motorIndex) const {
  if (motorIndex < NUM_MOTORS && _steppers[motorIndex]) {
    return _steppers[motorIndex]->isRunning();
  }
  return false;
}

void MotorHardware::setCurrentPosition(long pos) {
  for (int i = 0; i < NUM_MOTORS; i++) {
    if (_steppers[i]) {
      _steppers[i]->setCurrentPosition(pos);
    }
  }
}
