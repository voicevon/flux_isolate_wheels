#ifndef PINS_H
#define PINS_H

// ============================================================================
// ESP32 引脚定义 (共用 STEP, 8路独立 EN, 74HC595/165 串行 DIR/HOME)
// 与 flux_dealer 完全相同
// ============================================================================

// --- 共享的 STEP 引脚 ---
#define SHARED_STEP_PIN    5

// AccelStepper 需要的虚拟 DIR 引脚（不实际接线）
#define DUMMY_DIR_PIN      22

// --- 8路与门脉冲控制引脚 (高电平放行脉冲/电机旋转，低电平屏蔽脉冲/电机锁步。驱动芯片使能脚已接地常激活) ---
#define EN_PIN_0           12
#define EN_PIN_1           13
#define EN_PIN_2           14
#define EN_PIN_3           27
#define EN_PIN_4           26
#define EN_PIN_5           25
#define EN_PIN_6           33
#define EN_PIN_7           32

// --- 细分控制 (全核共用) ---
#define MS1_PIN            4
#define MS2_PIN            16
#define MS3_PIN            17

// --- 74HC595 (DIR 输出) & 74HC165 (HOME 输入) 共享 SPI ---
#define LATCH_PIN          18 // 锁存 (STCP / PL)
#define CLOCK_PIN          19 // 时钟 (SHCP / CP)
#define DIR_DATA_OUT       21 // HC595 串行数据输入 (DS)
#define HOME_DATA_IN       34 // HC165 串行数据输出 (Q7)

// --- 传感器 ---
#define ENTRANCE_SENSOR_PIN 39 // 光电开关，检测芦笋是否落入入口

// --- 其他 ---
#define LED_PIN            2

#endif // PINS_H
