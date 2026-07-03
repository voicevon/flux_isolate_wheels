#ifndef APP_HALL_DIAG_H
#define APP_HALL_DIAG_H

#include "AppBase.h"
#include <stdint.h>

class MotorHardware;
class ShiftRegisterBus;

class AppHallDiag : public AppBase {
public:
  AppHallDiag(MotorHardware& motorHardware, ShiftRegisterBus& spiBus);
  void setup() override;
  void loop() override;
  void stop() override;

private:
  MotorHardware& _motorHardware;
  ShiftRegisterBus& _spiBus;
  unsigned long _lastPrintTime;
  uint8_t _lastHomeState;
};

#endif // APP_HALL_DIAG_H
