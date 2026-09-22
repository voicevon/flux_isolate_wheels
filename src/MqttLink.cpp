#include "MqttLink.h"
#include "Logger.h"
#include <WiFi.h>

MqttLink* MqttLink::s_instance = NULL;

MqttLink::MqttLink(const char* ssid, const char* password,
                   const char* host, uint16_t port,
                   const char* user, const char* pass,
                   const char* topicPrefix)
  : _port(port), _wifiClient(), _mqtt(_wifiClient),
    _onCommand(NULL), _lastWifiAttempt(0), _lastMqttAttempt(0),
    _wifiLogged(false)
{
  strncpy(_ssid, ssid, sizeof(_ssid) - 1);       _ssid[sizeof(_ssid) - 1] = 0;
  strncpy(_password, password, sizeof(_password) - 1); _password[sizeof(_password) - 1] = 0;
  strncpy(_host, host, sizeof(_host) - 1);       _host[sizeof(_host) - 1] = 0;
  strncpy(_user, user, sizeof(_user) - 1);       _user[sizeof(_user) - 1] = 0;
  strncpy(_pass, pass, sizeof(_pass) - 1);       _pass[sizeof(_pass) - 1] = 0;
  strncpy(_prefix, topicPrefix, sizeof(_prefix) - 1); _prefix[sizeof(_prefix) - 1] = 0;
  s_instance = this;
}

void MqttLink::buildTopics() {
  snprintf(_topicCmd,   sizeof(_topicCmd),   "%s/%s/cmd",   _prefix, _deviceId);
  snprintf(_topicDone,  sizeof(_topicDone),  "%s/%s/done",  _prefix, _deviceId);
  snprintf(_topicState, sizeof(_topicState), "%s/%s/state", _prefix, _deviceId);
  snprintf(_topicLog,   sizeof(_topicLog),   "%s/%s/log",   _prefix, _deviceId);
}

void MqttLink::begin() {
  // 设备 ID：efuse MAC 低 16 位十六进制
  uint64_t mac = ESP.getEfuseMac();
  snprintf(_deviceId, sizeof(_deviceId), "%04X", (uint16_t)(mac & 0xFFFF));
  buildTopics();

  if (strlen(_ssid) == 0) {
    LOG_W("WiFi SSID 未配置 (config.h)，通信功能不可用");
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(_ssid, _password);

  _mqtt.setServer(_host, _port);
  _mqtt.setCallback(MqttLink::mqttCallback);
  _mqtt.setKeepAlive(15);
  _mqtt.setSocketTimeout(4);

  Logger_setSink(MqttLink::logSink);
  LOG_I("MQTT 目标: %s:%u, 设备ID: %s", _host, _port, _deviceId);
}

void MqttLink::connectMqtt() {
  char clientId[32];
  snprintf(clientId, sizeof(clientId), "flux-loader-%s", _deviceId);

  // 遗嘱消息：异常掉线时 broker 代发 offline (保留消息)
  bool ok = _mqtt.connect(clientId, _user, _pass,
                          _topicState, 0, true, "{\"state\":\"offline\"}");
  if (!ok) {
    LOG_W("MQTT 连接失败, state=%d", _mqtt.state());
    return;
  }

  // 上线后发布 idle 状态并订阅命令主题
  _mqtt.publish(_topicState, "{\"state\":\"idle\"}", true);
  _mqtt.subscribe(_topicCmd);
  LOG_I("MQTT 已连接, 订阅: %s", _topicCmd);
}

void MqttLink::loop() {
  if (strlen(_ssid) == 0) {
    return; // WiFi 未配置
  }

  unsigned long now = millis();

  if (WiFi.status() != WL_CONNECTED) {
    if (_wifiLogged) {
      LOG_W("WiFi 断开，重连中...");
      _wifiLogged = false;
    }
    if (now - _lastWifiAttempt >= 5000) {
      _lastWifiAttempt = now;
      WiFi.begin(_ssid, _password);
    }
    return;
  }

  if (!_wifiLogged) {
    _wifiLogged = true;
    LOG_I("WiFi 已连接, IP: %s", WiFi.localIP().toString().c_str());
  }

  if (!_mqtt.connected()) {
    if (now - _lastMqttAttempt >= 3000) {
      _lastMqttAttempt = now;
      connectMqtt();
    }
    return;
  }

  _mqtt.loop();
}

void MqttLink::publishDone() {
  if (_mqtt.connected()) {
    _mqtt.publish(_topicDone, "{\"event\":\"done\"}");
  }
}

void MqttLink::publishState(const char* state) {
  if (_mqtt.connected()) {
    char buf[48];
    snprintf(buf, sizeof(buf), "{\"state\":\"%s\"}", state);
    _mqtt.publish(_topicState, buf, true);
  }
}

void MqttLink::mqttCallback(char* topic, uint8_t* payload, unsigned int length) {
  MqttLink* self = s_instance;
  if (!self || !self->_onCommand) {
    return;
  }
  static char buf[256];
  unsigned int n = (length < sizeof(buf) - 1) ? length : (sizeof(buf) - 1);
  memcpy(buf, payload, n);
  buf[n] = 0;
  self->_onCommand(buf);
}

void MqttLink::logSink(const char* level, const char* line) {
  MqttLink* self = s_instance;
  if (!self || !self->_mqtt.connected()) {
    return;
  }
  char buf[300];
  snprintf(buf, sizeof(buf), "[%s] %s", level, line);
  self->_mqtt.publish(self->_topicLog, buf);
}
