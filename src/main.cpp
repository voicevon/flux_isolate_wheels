#include <Arduino.h>
#include <esp_task_wdt.h>
#include "pins.h"
#include "config.h"
#include "Logger.h"
#include "MotorHardware.h"
#include "ShiftRegisterBus.h"
#include "AppBase.h"
#include "MqttLink.h"
#include "AppProduction.h"
#include "AppMotorDiag.h"
#include "AppHallDiag.h"

// 电机引脚列表
const uint8_t motorStepPins[NUM_MOTORS] = {
  STEP_PIN_0, STEP_PIN_1, STEP_PIN_2, STEP_PIN_3,
  STEP_PIN_4, STEP_PIN_5, STEP_PIN_6, STEP_PIN_7
};

// 硬件驱动对象
MotorHardware motorHardware(motorStepPins);
ShiftRegisterBus spiBus(LATCH_PIN, CLOCK_PIN, DIR_DATA_OUT, HOME_DATA_IN);

// 通信对象 (WiFi + MQTT)
MqttLink mqttLink(WIFI_SSID, WIFI_PASSWORD, MQTT_HOST, MQTT_PORT,
                  MQTT_USER, MQTT_PASS, MQTT_TOPIC_PREFIX);

// APP 实例
AppProduction appProduction(motorHardware, spiBus, mqttLink);
AppMotorDiag appMotorDiag(motorHardware, spiBus);
AppHallDiag appHallDiag(motorHardware, spiBus);

// 当前运行的模式
enum RunMode {
  MODE_PRODUCTION,
  MODE_DIAG_MOTOR,
  MODE_DIAG_HALL
};

RunMode currentMode = MODE_PRODUCTION;
AppBase* activeApp = &appProduction;

// 看门狗超时时间（秒）
#define WDT_TIMEOUT_S  10

// 切换模式函数
void switchMode(RunMode newMode) {
  if (activeApp) {
    activeApp->stop();
  }

  currentMode = newMode;

  switch (currentMode) {
    case MODE_PRODUCTION:
      activeApp = &appProduction;
      break;
    case MODE_DIAG_MOTOR:
      activeApp = &appMotorDiag;
      break;
    case MODE_DIAG_HALL:
      activeApp = &appHallDiag;
      break;
  }

  if (activeApp) {
    activeApp->setup();
  }
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(0, INPUT_PULLUP); // BOOT 按键输入，外部带上拉

  Serial.begin(115200);
  while (!Serial) {} // 等待串口就绪

  LOG_I("--- ESP32 物料输送系统 (Flux Loader) ---");

  // 启用看门狗定时器
  esp_task_wdt_init(WDT_TIMEOUT_S, true);
  esp_task_wdt_add(NULL);
  LOG_I("看门狗已启用 (超时 %d 秒)", WDT_TIMEOUT_S);

  // 初始化共享的细分控制引脚
  pinMode(MS1_PIN, OUTPUT);
  pinMode(MS2_PIN, OUTPUT);
  pinMode(MS3_PIN, OUTPUT);
  
  // 设置细分为真值表规定的 1/16 细分 (MS1=H, MS2=H, MS3=H)
  digitalWrite(MS1_PIN, HIGH);
  digitalWrite(MS2_PIN, HIGH);
  digitalWrite(MS3_PIN, HIGH);
  LOG_I("步进电机驱动细分初始化: 1/16");

  // 初始化硬件模块
  motorHardware.begin(STEPPER_MAX_SPEED, STEPPER_ACCELERATION);
  spiBus.begin();

  // 初始化通信 (WiFi + MQTT，内部非阻塞，断线由 loop 自动重连)
  mqttLink.begin();

  // 默认启动生产模式
  activeApp->setup();
  LOG_I("初始化完成，进入正常生产模式。");
}

void loop() {
  // 喂看门狗
  esp_task_wdt_reset();

  // GPIO 0 按键检测逻辑（防抖，长按切换模式，短按传递给诊断模块）
  static unsigned long buttonPressTime = 0;
  static bool longPressTriggered = false;
  static bool lastButtonState = HIGH;

  bool currentButtonState = digitalRead(0);
  if (currentButtonState != lastButtonState) {
    delay(10); // 10ms 消抖
    currentButtonState = digitalRead(0);
  }

  if (currentButtonState == LOW) {
    if (buttonPressTime == 0) {
      buttonPressTime = millis();
      longPressTriggered = false;
    } else {
      unsigned long duration = millis() - buttonPressTime;
      // 长按 1.5s 切换 APP
      if (duration >= 1500 && !longPressTriggered) {
        longPressTriggered = true;
        
        RunMode nextMode;
        if (currentMode == MODE_PRODUCTION) {
          nextMode = MODE_DIAG_MOTOR;
        } else if (currentMode == MODE_DIAG_MOTOR) {
          nextMode = MODE_DIAG_HALL;
        } else {
          nextMode = MODE_PRODUCTION;
        }
        
        switchMode(nextMode);
      }
    }
  } else {
    if (buttonPressTime != 0) {
      unsigned long duration = millis() - buttonPressTime;
      if (!longPressTriggered && duration >= 50) {
        // 短按触发诊断模式下的换向/换电机等功能
        if (currentMode == MODE_DIAG_MOTOR) {
          appMotorDiag.nextMotor();
        }
      }
      buttonPressTime = 0;
      longPressTriggered = false;
    }
  }
  lastButtonState = currentButtonState;

  // 维护 WiFi/MQTT 连接与收包
  mqttLink.loop();

  // 运行当前的 APP 逻辑
  if (activeApp) {
    activeApp->loop();
  }

  // 物理心跳指示灯闪烁控制 (仅在非霍尔诊断模式下慢闪，霍尔诊断由 AppHallDiag 独占 LED)
  if (currentMode != MODE_DIAG_HALL) {
    static unsigned long lastBeat = 0;
    if (millis() - lastBeat >= 1000) {
      lastBeat = millis();
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    }
  }
}
