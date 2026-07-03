#ifndef CONFIG_H
#define CONFIG_H

#include <limits.h>
#include <stdint.h>

// ============================================================================
// 硬件配置：ESP32
// ============================================================================

// --- 细分配置（MS1/MS2/MS3 引脚已连至 MCU GPIO，由软件控制）---
// 板载驱动芯片：A4988 / HR4988（包含 MS1, MS2, MS3）
// A4988 真值表：
//   MS1=L MS2=L MS3=L -> 全步(1)     MS1=H MS2=L MS3=L -> 1/2步
//   MS1=L MS2=H MS3=L -> 1/4步     MS1=H MS2=H MS3=L -> 1/8步
//   MS1=H MS2=H MS3=H -> 1/16步
// 引脚定义见 pins.h（MS1_PIN / MS2_PIN / MS3_PIN）
#define MICROSTEP_RESOLUTION  16    // 软件目标细分：1 | 2 | 4 | 8 | 16

#define NUM_MOTORS            8     // 电机数量

// --- 步进电机几何参数 ---
#define MOTOR_FULL_STEPS      200   // 每转整步数（1.8°/步电机）
#define GEAR_RATIO            1     // 减速比 1:1（托架转轮直驱或 1:1 减速）

// 旋转度数对应的脉冲步数定义：
// 90° 所需步数 = (整步数 * 细分 * 减速比) / 4 = 800
#define STEPS_PER_90DEG  ((long)(MOTOR_FULL_STEPS * MICROSTEP_RESOLUTION * GEAR_RATIO) / 4)
// 22.5° 所需步数 = (整步数 * 细分 * 减速比) / 16 = 200
#define STEPS_PER_22_5DEG ((long)(MOTOR_FULL_STEPS * MICROSTEP_RESOLUTION * GEAR_RATIO) / 16)

// --- 运动参数 ---
#define STEPPER_MAX_SPEED     3200.0f   // 最大速度（步/秒）
#define STEPPER_ACCELERATION  6400.0f   // 加速度（步/秒²）

// --- 时序参数 ---
#define DEBOUNCE_MS       50UL  // 按键防抖采样间隔（毫秒）

#endif // CONFIG_H
