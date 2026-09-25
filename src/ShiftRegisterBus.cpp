#include "ShiftRegisterBus.h"
#include "Logger.h"

// 二进制格式化辅助：输出 "b7..b0" 共 8 字符
static const char* byteToBin(uint8_t v, char* buf) {
  for (int i = 7; i >= 0; i--) {
    buf[7 - i] = (v & (1 << i)) ? '1' : '0';
  }
  buf[8] = '\0';
  return buf;
}

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
  char bin[9];
  // LOG_D("595 入口: dir_state=0b%s (0x%02X)", byteToBin(dir_state, bin), dir_state);

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

    // 写 HC595 数据 (低位先发：bit N 移 8 拍后落在 Q(7-N)，对应接线 Q0=8号轮 ... Q7=1号轮)
    // digitalWrite(_dataOutPin, (dir_state & (1 << (7 - i))) ? HIGH : LOW);
    uint8_t bit = (dir_state >> i) & 1;
    digitalWrite(_dataOutPin, bit ? HIGH : LOW);
    // LOG_D("595: i=%d 判断(dir_state>>%d)&1=%u → GPIO%d=%s",
    //       i, i, bit, _dataOutPin, bit ? "HIGH" : "LOW");
    
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
