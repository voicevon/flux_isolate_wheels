# 临时测试脚本：验证 flux_loader MQTT 协议（单电机 + 多电机调试命令）
import json
import sys
import time

import paho.mqtt.client as mqtt

BROKER = "voicevon.vicp.io"
PORT = 1883
USER = "von"
PASS = "von123456"
DEVID = "F8EC"
BASE = f"flux/loader/{DEVID}"

events = []
subscribed = {"state": False, "done": False, "log": False}


def on_connect(client, userdata, flags, rc, props=None):
    print(f"[连接] rc={rc}")
    for t in ("state", "done", "log"):
        client.subscribe(f"{BASE}/{t}")
        subscribed[t] = True


def on_message(client, userdata, msg):
    payload = msg.payload.decode("utf-8", "replace")
    kind = msg.topic.rsplit("/", 1)[-1]
    retained = " [retained]" if msg.retain else ""
    line = f"[{kind}{retained}] {payload}"
    print(line)
    events.append((kind, payload))


def main():
    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, protocol=mqtt.MQTTv311)
    client.username_pw_set(USER, PASS)
    client.on_connect = on_connect
    client.on_message = on_message

    print(f"[连接中] {BROKER}:{PORT} ...")
    client.connect(BROKER, PORT, 15)
    client.loop_start()

    # 等待连接 + 收保留消息（state 初始化）
    deadline = time.time() + 10
    while time.time() < deadline and not all(subscribed.values()):
        time.sleep(0.1)
    time.sleep(2)

    print("\n[发送] motor 调试命令: 5号电机 正转 90°")
    cmd = {"cmd": "motor", "motor": 5, "dir": 1, "angle": 90}
    client.publish(f"{BASE}/cmd", json.dumps(cmd))

    # 等待 done（调试低速 90°=800 步，约 1-2 秒运动时间）
    deadline = time.time() + 12
    got_done = False
    while time.time() < deadline and not got_done:
        for kind, payload in events:
            if kind == "done" and '"cmd":"motor"' in payload.replace(" ", ""):
                got_done = True
        time.sleep(0.1)

    time.sleep(1)
    print("\n[结果]")
    print(f"  收到 done(motor): {'是' if got_done else '否'}")
    states = [p for k, p in events if k == "state"]
    print(f"  state 序列: {states}")
    motor_ok = got_done

    # ===== multi 多电机调试：1号正转90°、2号反转45°、3号正转90°，其余不动 =====
    # 预期现象：3 台电机同时启动、同时结束（45° 者先停），而不是依次转动
    events.clear()
    print("\n[发送] multi 多电机调试: [90, -45, 90, 0, 0, 0, 0, 0]")
    cmd = {"cmd": "multi", "angles": [90, -45, 90, 0, 0, 0, 0, 0]}
    client.publish(f"{BASE}/cmd", json.dumps(cmd))

    deadline = time.time() + 15
    got_done = False
    while time.time() < deadline and not got_done:
        for kind, payload in events:
            if kind == "done" and '"cmd":"multi"' in payload.replace(" ", ""):
                got_done = True
        time.sleep(0.1)

    time.sleep(1)
    print("\n[结果]")
    print(f"  收到 done(multi): {'是' if got_done else '否'}")
    states = [p for k, p in events if k == "state"]
    print(f"  state 序列: {states}")
    multi_ok = got_done

    client.loop_stop()
    client.disconnect()
    sys.exit(0 if (motor_ok and multi_ok) else 1)


if __name__ == "__main__":
    main()
