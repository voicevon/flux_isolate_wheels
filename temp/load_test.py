# -*- coding: utf-8 -*-
"""临时测试：发送 load 命令并监听 done/state/log 应答。
用法: python load_test.py [n1 n2 ... n8]   (缺省为文档例1: 1 0 2 0 1 0 0 3)
"""
import sys, json, time
import paho.mqtt.client as mqtt

HOST, PORT, USER, PASS = "voicevon.vicp.io", 1883, "von", "von123456"
DEVID = "F8EC"
BASE = f"flux/loader/{DEVID}"

counts = [1, 0, 2, 0, 1, 0, 0, 3]
if len(sys.argv) == 9:
    counts = [int(x) for x in sys.argv[1:9]]

def on_connect(c, u, f, rc):
    print("connected rc=", rc)
    c.subscribe(f"{BASE}/#")

def on_message(c, u, m):
    print(f"[{time.strftime('%H:%M:%S')}] {m.topic}: {m.payload.decode(errors='replace')}")

c = mqtt.Client()
c.username_pw_set(USER, PASS)
c.on_connect = on_connect
c.on_message = on_message
c.connect(HOST, PORT, 60)
c.loop_start()
time.sleep(2)

payload = json.dumps({"cmd": "load", "counts": counts})
print(">>>", payload)
c.publish(f"{BASE}/cmd", payload, qos=0)

time.sleep(15)
c.loop_stop()
