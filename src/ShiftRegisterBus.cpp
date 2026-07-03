#include "ShiftRegisterBus.h"

ShiftRegisterBus::ShiftRegisterBus(uint8_t latchPin, uint8_t clockPin, uint8_t dataOutPin, uint8_t dataInPin)
  : _latchPin(latchPin), _clockPin(clockPin), _dataOutPin(dataOutPin), _dataInPin(dataInPin) {
}

void ShiftRegisterBus::begin() {
  pinMode(_latchPin, OUTPUT);
  digitalWrite(_latchPin, HIGH);
  
  pinMode(_clockPin, OUTPUT);
  digitalWrite(_clockPin, HIGH);
  
  pinMode(_dataOutPin, OUTPUT);
  digitalWrite(_dataOutPin, LOW);
  
  pinMode(_dataInPin, INPUT);
}

uint8_t ShiftRegisterBus::transfer(uint8_t dir_state) {
  // 1. 锁存当前 HC165 引脚状态到移位寄存器，并将上一次 HC595 数据推到输出端口
  digitalWrite(_latchPin, LOW);
  delayMicroseconds(1);
  digitalWrite(_latchPin, HIGH);
  delayMicroseconds(1);

  uint8_t home_state = 0;
  for (int i = 0; i < 8; i++) {
    // 读取 HC165 数据 (通常 Q7 优先输出，对应最高位)
    if (digitalRead(_dataInPin) == HIGH) {
      home_state |= (1 << (7 - i));
    }
    
    // 写 HC595 数据 (高位先发，保证最后移入 Q0 的是最低位，以此类推)
    digitalWrite(_dataOutPin, (dir_state & (1 << (7 - i))) ? HIGH : LOW);
    
    // 时钟脉冲，移位
    digitalWrite(_clockPin, LOW);
    delayMicroseconds(1);
    digitalWrite(_clockPin, HIGH);
    delayMicroseconds(1);
  }

  // 2. 再次锁存，将刚才移入 HC595 的数据立即推到输出端口
  digitalWrite(_latchPin, LOW);
  delayMicroseconds(1);
  digitalWrite(_latchPin, HIGH);

  return home_state;
}
