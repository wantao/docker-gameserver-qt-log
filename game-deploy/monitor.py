# -*- coding: utf-8 -*-
import os
import time
import requests

# ===================== 配置 =====================
LOG_FILE = "/home/game-deploy/log/server_1001/game.log"
OFFSET_FILE = "/home/game-deploy/log/server_1001/offset.txt"

# 钉钉机器人 Webhook（把你自己的替换进来）
DING_WEBHOOK = "https://oapi.dingtalk.com/robot/send?access_token=cccc"

# 告警关键字（命中就发钉钉）
ALERT_KEYWORDS = ["ERROR", "Exception", "崩溃", "失败", "异常", "错误", "error_count", "LISTEN"]
# =================================================

def get_current_offset():
    if os.path.exists(OFFSET_FILE):
        try:
            with open(OFFSET_FILE, "r") as f:
                return int(f.read().strip())
        except:
            return 0
    return 0

def save_offset(offset):
    with open(OFFSET_FILE, "w") as f:
        f.write(str(offset))

def send_ding_alert(msg):
    """发送告警到钉钉群"""
    headers = {"Content-Type": "application/json"}
    data = {
        "msgtype": "text",
        "text": {
            # 这行修复了 f-string 问题
            "content": "【游戏服日志告警error】\n%s\n告警时间：%s" % (msg, time.ctime())
        }
    }
    try:
        requests.post(DING_WEBHOOK, json=data, headers=headers, timeout=3)
    except Exception as e:
        print("钉钉发送失败:", e)

def monitor_log():
    offset = get_current_offset()

    if not os.path.exists(LOG_FILE):
        return

    file_size = os.path.getsize(LOG_FILE)
    if file_size < offset:
        offset = 0

    # 只读模式打开，不会和游戏服写日志产生竞争
    # Python 2.6 兼容打开方式（无 encoding 参数）
    f = open(LOG_FILE, "r")
    f.seek(offset)
    lines = f.readlines()
    new_offset = f.tell()
    f.close()
  

    # 处理新增行
    for line in lines:
        line = line.strip()
        if not line:
            continue

        # 控制台输出
        print(line)

        # 关键字匹配告警
        for kw in ALERT_KEYWORDS:
            if kw in line:
                print("\n⚠️  发现告警：", line)
                send_ding_alert(line)
                break

    # 保存偏移量（断点续读核心）
    save_offset(new_offset)

if __name__ == "__main__":
    print("日志监控已启动（断点续读 + 钉钉告警）...")
    while True:
        try:
            monitor_log()
        except Exception as e:
            print("监控异常:", e)
        time.sleep(5)
