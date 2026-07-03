#include "MotorHardware.h"
#include "pins.h"

// 全局静态的 FastAccelStepperEngine 实例
static FastAccelStepperEngine engine = FastAccelStepperEngine();

MotorHardware::MotorHardware(const uint8_t* enablePins, uint8_t numMotors)
  : _numMotors(numMotors), _stepper(NULL)
{
  _enablePins = new uint8_t[_numMotors];
  for (int i = 0; i < _numMotors; i++) {
    _enablePins[i] = enablePins[i];
  }
}

MotorHardware::~MotorHardware() {
  delete[] _enablePins;
}

void MotorHardware::begin(float maxSpeed, float acceleration) {
  // 初始化所有 EN 引脚为屏蔽状态 (LOW)
  for (int i = 0; i < _numMotors; i++) {
    pinMode(_enablePins[i], OUTPUT);
    digitalWrite(_enablePins[i], LOW);
  }

  engine.init();
  _stepper = engine.stepperConnectToPin(SHARED_STEP_PIN);
  if (_stepper) {
    _stepper->setDirectionPin(DUMMY_DIR_PIN);
    _stepper->setSpeedInHz(maxSpeed);
    _stepper->setAcceleration(acceleration);
  }
}

void MotorHardware::setMaxSpeed(float speed) {
  if (_stepper) {
    _stepper->setSpeedInHz(speed);
  }
}

void MotorHardware::setAcceleration(float accel) {
  if (_stepper) {
    _stepper->setAcceleration(accel);
  }
}

void MotorHardware::setEnableMask(uint8_t enableMask) {
  for (int i = 0; i < _numMotors; i++) {
    // 1 表示放行 (HIGH), 0 表示屏蔽 (LOW)
    bool enabled = (enableMask & (1 << i)) != 0;
    digitalWrite(_enablePins[i], enabled ? HIGH : LOW);
  }
}

void MotorHardware::setMotorEnabled(uint8_t motorIndex, bool enabled) {
  if (motorIndex < _numMotors) {
    digitalWrite(_enablePins[motorIndex], enabled ? HIGH : LOW);
  }
}

void MotorHardware::startMove(long steps) {
  if (_stepper) {
    _stepper->move(steps);
  }
}

void MotorHardware::stop() {
  if (_stepper) {
    _stepper->stopMove();
  }
}

bool MotorHardware::isMoving() const {
  if (_stepper) {
    return _stepper->isRunning();
  }
  return false;
}

void MotorHardware::setCurrentPosition(long pos) {
  if (_stepper) {
    _stepper->setCurrentPosition(pos);
  }
}
