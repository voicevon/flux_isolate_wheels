#ifndef APP_BASE_H
#define APP_BASE_H

class AppBase {
public:
    virtual ~AppBase() {}
    virtual void setup() = 0;
    virtual void loop() = 0;
    virtual void stop() = 0; // 退出模式时的清理工作（确保电机停止、释放使能）
};

#endif // APP_BASE_H
