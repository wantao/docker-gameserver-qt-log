# docker-gameserver-qt-log
# 包含两个目录 game-deploy和LogClient
#前置条件
#centos7上
    #安装docker
        yum install -y docker
        systemctl start docker
        systemctl enable docker
        #配置镜像
            vim /etc/docker/daemon.json
            {
            "registry-mirrors": [
                "https://docker.1ms.run",
                "https://docker-0.unsee.tech",
                "https://docker.m.daocloud.io"
            ],
            "dns": ["8.8.8.8", "114.114.114.114"]
            }
        #重启docker
            sudo systemctl daemon-reload
            sudo systemctl restart docker
    #安装redis
        #1. 安装 Redis
            yum install -y epel-release
            yum install -y redis hiredis hiredis-devel
        #2. 修改配置，允许外部连接
            sed -i 's/bind 127.0.0.1/bind 0.0.0.0/' /etc/redis.conf
            sed -i 's/protected-mode yes/protected-mode no/' /etc/redis.conf
        #3. 启动
            systemctl start redis
            systemctl enable redis
        #4. 防火墙开放 6379
            firewall-cmd --add-port=6379/tcp --permanent
            firewall-cmd --reload
    #安装mysql
        #1.安装 MySQL
            yum install -y https://dev.mysql.com/get/mysql80-community-release-el7-3.noarch.rpm
            yum install -y mysql-community-server mysql-community-devel
        #2. 启动并设置开机自启
            systemctl start mysqld
            systemctl enable mysqld
        #3. 初始化密码（创建游戏服账号）
            # 获取临时密码
            grep 'temporary password' /var/log/mysqld.log

            # 登录修改密码
            mysql -uroot -p

            # 进入 MySQL 后执行：
            ALTER USER 'root'@'localhost' IDENTIFIED BY '123456';
            create database game_db;
            use game_db;
            create table player(id int,name varchar(20));
            insert into player values(1001,'testplayer');

            # 允许外部IP连接（关键！让Docker容器能连）
            CREATE USER 'root'@'%' IDENTIFIED BY '123456';
            GRANT ALL ON *.* TO 'root'@'%';
            ALTER USER 'root'@'%' IDENTIFIED BY '123456';
            flush privileges;
            exit
        #4. 防火墙开放 3306
            firewall-cmd --add-port=3306/tcp --permanent
            firewall-cmd --reload
#game-deploy
    运行在centos7的docker容器内，目录结构如下
    config\server_1001
        存放1001区的配置
    config\server_1002
        存放1002区的配置
    log\server_1001
        存放1001区的日志
    log\server_1002
        存放1002区的日志
    mysql-lib\
        存放服务器程序在docker里运行时所依赖的mysql相关库
    redis-lib\
        存放服务器程序在docker里运行时所依赖的redis相关库
    build.sh
        编译服务器程序的脚本
    Dockerfile
        自动化地创建 Docker 镜像的指令
    gameserver.cpp
        服务器程序源代码，监听在8002端口
    docker.txt
        docker用法
    logrotate.txt
        服务器日志切割用法
    monitor.py
        用于监控服务器程序日志，有异常日志，发送告警到钉钉群，需要
        提前在钉钉群创建机器人，获取到钉钉机器人 Webhook地址，配置进监控脚本里，
        用python2的语法

    run.sh
        批量启动服务器程序
    test.sh
        docker pull centos:7报错时，试试这个脚本

#LogClient
    前提条件
        #需要下载安装QT，地址https://download.qt.io/archive/qt/5.14/5.14.2/qt-opensource-windows-x86-5.14.2.exe
        #用迅雷下
    文件结构
        LogClient.pro
        main.cpp
        mainwindow.cpp
        mainwindow.h
    功能
        选择区日志时，ssh连接服务器主机，并在主机开启日记发送监听端口，同时连接该端口实时显示日志
        1001区的8011日志端口，连接服务器1002区的8012日志端口，
        docker exec game_1001 tail -f /game/log/game.log | nc -lk 8011 &
        docker exec game_1002 tail -f /game/log/game.log | nc -lk 8012 &

#game-deploy.png
    服务器启动效果图
#LogClient.jpg
    qt日志查看效果图
#dingding.png
    日志实时告警图
    
    