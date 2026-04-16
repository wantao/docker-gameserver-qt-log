#!/bin/bash
echo "编译游戏服..."
g++ gameserver.cpp -o gameserver -lmysqlclient -lhiredis -pthread

echo "编译完成！"
