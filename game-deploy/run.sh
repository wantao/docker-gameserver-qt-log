#!/bin/bash

if [ $# -ne 1 ]; then
    echo "用法: ./run.sh <1001|1002>"
    exit 1
fi

SERVER_ID=$1
PORT=800${SERVER_ID: -1}  # 1001→8001 1002→8002

LOG_DIR=$PWD/log/server_$SERVER_ID
CONFIG_DIR=$PWD/config/server_$SERVER_ID

mkdir -p $LOG_DIR $CONFIG_DIR

echo "==== 启动区服: $SERVER_ID 端口: $PORT ===="

# 停止旧容器
docker stop game_$SERVER_ID >/dev/null 2>&1
docker rm game_$SERVER_ID >/dev/null 2>&1

# 构建
docker build -t game:$SERVER_ID .

# 启动（关键：独立挂载）
docker run -d \
  --name game_$SERVER_ID \
  -p $PORT:8002 \
  -v $CONFIG_DIR:/game/config \
  -v $LOG_DIR:/game/log \
  game:$SERVER_ID \
  /game/gameserver $SERVER_ID

docker ps | grep game_
