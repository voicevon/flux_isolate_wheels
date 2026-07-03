#ifndef SHIFT_REGISTER_BUS_H
#define SHIFT_REGISTER_BUS_H

#include <Arduino.h>

class ShiftRegisterBus {
public:
  ShiftRegisterBus(uint8_t latchPin, uint8_t clockPin, uint8_t dataOutPin, uint8_t dataInPin);

  void begin();
  
  // 发送 dirState 并读取 homeState
  uint8_t transfer(uint8_t dir_state);

private:
  uint8_t _latchPin;
  uint8_t _clockPin;
  uint8_t _dataOutPin;
  uint8_t _dataInPin;
};

#endif // SHIFT_REGISTER_BUS_H
