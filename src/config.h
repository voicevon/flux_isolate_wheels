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
#define GEAR_RATIO            (54.0 / 18.0)   // 减速比 = 从动轮 54 齿 / 主动轮 18 齿 = 3.0

// 旋转度数对应的脉冲步数定义（电机轴步数，减速前）：
// 90° 所需步数 = (整步数 * 细分 * 减速比) / 4 = 2400
#define STEPS_PER_90DEG  ((long)(MOTOR_FULL_STEPS * MICROSTEP_RESOLUTION * GEAR_RATIO) / 4)
// 60° 所需步数 = (整步数 * 细分 * 减速比) / 6 = 1600
#define STEPS_PER_60DEG  ((long)(MOTOR_FULL_STEPS * MICROSTEP_RESOLUTION * GEAR_RATIO) / 6)
// 30° 所需步数 = (整步数 * 细分 * 减速比) / 12 = 800
#define STEPS_PER_30DEG  ((long)(MOTOR_FULL_STEPS * MICROSTEP_RESOLUTION * GEAR_RATIO) / 12)
// 28° = 9600 * 28 / 360 ≈ 747 步（四舍五入）
#define STEPS_PER_28DEG  ((long)(MOTOR_FULL_STEPS * MICROSTEP_RESOLUTION * GEAR_RATIO * 28.0 / 360.0 + 0.5))
// 32° = 9600 * 32 / 360 ≈ 853 步（四舍五入）
#define STEPS_PER_32DEG  ((long)(MOTOR_FULL_STEPS * MICROSTEP_RESOLUTION * GEAR_RATIO * 32.0 / 360.0 + 0.5))

// --- 逻辑方向 → 物理方向映射 ---
// 逻辑正转 = 物料输送方向。若某电机因安装朝向与逻辑定义相反，将其对应位置 1（求反）。
// 位0 = 1号电机 ... 位7 = 8号电机；全部相反填 0xFF；默认全部不反 0x00。
#define DIR_INVERT_MASK       0b11111111

// --- 运动参数 ---
#define STEPPER_MAX_SPEED     3200.0f   // 最大速度（步/秒）
#define STEPPER_ACCELERATION  6400.0f   // 加速度（步/秒²）
#define STEPPER_DIAG_SPEED     800.0f   // 调试模式电机最大速度（步/秒）
#define STEPPER_DIAG_ACCEL    1600.0f   // 调试模式电机加速度（步/秒²）

// --- 时序参数 ---
#define DEBOUNCE_MS       50UL  // 按键防抖采样间隔（毫秒）

// ============================================================================
// 通信配置：WiFi + MQTT（蓝牙已移除，经典蓝牙与 WiFi 不能共存）
// ============================================================================
#define WIFI_SSID         "Perfect"      // TODO: 填入 WiFi 名称
#define WIFI_PASSWORD     "12344321"      // TODO: 填入 WiFi 密码

#define MQTT_HOST         "voicevon.vicp.io"
#define MQTT_PORT         1883
#define MQTT_USER         "von"
#define MQTT_PASS         "von123456"
#define MQTT_TOPIC_PREFIX "flux/loader"   // 主题前缀，设备 ID (MAC后4位) 自动追加

#endif // CONFIG_H
