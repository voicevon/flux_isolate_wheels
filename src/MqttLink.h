#ifndef MQTT_LINK_H
#define MQTT_LINK_H

#include <Arduino.h>
#include <WiFiClient.h>
#include <PubSubClient.h>

// WiFi + MQTT 通信封装（已批准的新类）：
// - WiFi STA 连接维护，断线自动重连
// - MQTT 自动重连、遗嘱消息 (LWT) 上下线、状态保留消息
// - 命令主题订阅分发（回调注册）、完成/状态/日志发布
// 主题结构（devid 为 MAC 后 4 位十六进制）：
//   {prefix}/{devid}/cmd    下行命令 {"cmd":"load","counts":[n1..n8]}
//   {prefix}/{devid}/done   节拍完成应答 {"event":"done"}
//   {prefix}/{devid}/state  运行状态保留消息 {"state":"idle|running|offline"}
//   {prefix}/{devid}/log    远程日志（文本）
class MqttLink {
public:
  // 收到下行命令时的回调，参数为原始 JSON 文本
  typedef void (*CommandFn)(const char* payload);

  MqttLink(const char* ssid, const char* password,
           const char* host, uint16_t port,
           const char* user, const char* pass,
           const char* topicPrefix);

  void begin();

  // 需在主循环持续调用：维护 WiFi/MQTT 连接并处理收包
  void loop();

  bool connected() { return _mqtt.connected(); }

  void onCommand(CommandFn fn) { _onCommand = fn; }

  // 发布节拍完成应答
  void publishDone();

  // 发布运行状态 (保留消息)，state 取值如 "idle" / "running"
  void publishState(const char* state);

private:
  void buildTopics();
  void connectMqtt();
  static void mqttCallback(char* topic, uint8_t* payload, unsigned int length);
  static void logSink(const char* level, const char* line);

  char _ssid[33];
  char _password[65];
  char _host[64];
  uint16_t _port;
  char _user[33];
  char _pass[65];
  char _prefix[48];

  char _deviceId[8];
  char _topicCmd[80];
  char _topicDone[80];
  char _topicState[80];
  char _topicLog[80];

  WiFiClient _wifiClient;
  PubSubClient _mqtt;
  CommandFn _onCommand;
  unsigned long _lastWifiAttempt;
  unsigned long _lastMqttAttempt;
  bool _wifiLogged;

  static MqttLink* s_instance;
};

#endif // MQTT_LINK_H
