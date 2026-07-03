#include "AppHallDiag.h"
#include "MotorHardware.h"
#include "ShiftRegisterBus.h"
#include "pins.h"
#include "config.h"
#include "Logger.h"
#include <Arduino.h>

AppHallDiag::AppHallDiag(MotorHardware& motorHardware, ShiftRegisterBus& spiBus) 
  : _motorHardware(motorHardware), _spiBus(spiBus), _lastPrintTime(0), _lastHomeState(0xFF) {}

void AppHallDiag::setup() {
  LOG_I("--- [诊断模式] 霍尔限位与传感器诊断启动 ---");
  LOG_I("用磁铁靠近各轴霍尔开关进行测试，或遮挡入口光电传感器。");
  LOG_I("当有任何传感器被触发时，板载 LED 将会亮起。");
  
  _lastPrintTime = 0;
  _lastHomeState = 0xFF;
  
  // 诊断霍尔时屏蔽所有电机脉冲，保障安全
  _motorHardware.setEnableMask(0x00);
  _motorHardware.stop();

  pinMode(ENTRANCE_SENSOR_PIN, INPUT_PULLUP);
}

void AppHallDiag::stop() {
  LOG_I("--- [诊断模式] 霍尔限位与传感器诊断结束 ---");
  // 恢复 LED 电平为低电平（默认状态）
  digitalWrite(LED_PIN, LOW);
}

void AppHallDiag::loop() {
  // 确保处于屏蔽状态
  _motorHardware.setEnableMask(0x00);
  
  // 从 SPI 总线（74HC165）读取当前霍尔状态
  uint8_t home_state = _spiBus.transfer(0x00);
  
  // 读取入口光电传感器状态 (假设低电平触发遮挡)
  bool entranceTriggered = (digitalRead(ENTRANCE_SENSOR_PIN) == LOW);
  
  // 如果状态发生改变，或者超过 500ms，则打印一次状态
  if (home_state != _lastHomeState || millis() - _lastPrintTime >= 500) {
    _lastPrintTime = millis();
    _lastHomeState = home_state;
    
    Serial.print("[诊断模式] 传感器状态: ");
    for (int i = 0; i < NUM_MOTORS; i++) {
      // 霍尔触发时通常被拉低为 0
      bool triggered = ((home_state & (1 << i)) == 0);
      Serial.printf("M%d:%s ", i, triggered ? "TRG" : "---");
    }
    Serial.printf("| 入口:%s", entranceTriggered ? "TRG" : "---");
    Serial.println();
  }
  
  // 如果有任何一个传感器被触发，则点亮开发板板载 LED
  bool anyTriggered = (home_state != 0xFF) || entranceTriggered;
  digitalWrite(LED_PIN, anyTriggered ? HIGH : LOW);
}
