#ifndef PINS_H
#define PINS_H

// ============================================================================
// ESP32 引脚定义 (8路独立 STEP, EN 接地常使能, 74HC595/165 串行 DIR/HOME)
// ============================================================================

// --- 8路独立 STEP 引脚 (硬件排线首路为 8 号轮，与 74HC595 输出 Q0~Q7 → 轮 8~1 一致) ---
#define STEP_PIN_0         32
#define STEP_PIN_1         33
#define STEP_PIN_2         25
#define STEP_PIN_3         26
#define STEP_PIN_4         27
#define STEP_PIN_5         14
#define STEP_PIN_6         12
#define STEP_PIN_7         13

// 方向不经由 GPIO：真实方向由 74HC595 串行锁存输出，
// FastAccelStepper 不设置方向脚（共享虚拟方向脚会触发库内串行化）

// 驱动芯片使能脚 (EN) 未接 GPIO，直接接地：永远使能，无需软件控制

// --- 细分控制 (全核共用) ---
#define MS1_PIN            4
#define MS2_PIN            16
#define MS3_PIN            17

// --- 74HC595 (DIR 输出) & 74HC165 (HOME 输入) 共享 SPI ---
#define LATCH_PIN          18 // 锁存 (STCP / PL)
#define CLOCK_PIN          19 // 移位 (SHCP / CP)
#define DIR_DATA_OUT       21 // HC595 串行数据输入 (DS)
#define HOME_DATA_IN       34 // HC165 串行数据输出 (Q7)

// --- 传感器 ---
#define ENTRANCE_SENSOR_PIN 39 // 光电开关，检测芦笋是否落入入口

// --- 其他 ---
#define LED_PIN            2

#endif // PINS_H
