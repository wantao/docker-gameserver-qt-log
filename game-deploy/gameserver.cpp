#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <hiredis/hiredis.h>
#include <mysql/mysql.h>
#include <cstring>

// 全局日志锁（多线程写日志必须加锁）
pthread_mutex_t log_mutex;

// 日志写入函数（线程安全）
void write_log(const char* level, const char* msg) {
    // 加锁 → 多个线程不能同时写文件
    pthread_mutex_lock(&log_mutex);

    FILE* fp = fopen("/game/log/game.log", "a");
    if (fp == NULL) {
        printf("打开文件失败\n");
        pthread_mutex_unlock(&log_mutex);
        return;
    }

    time_t now = time(NULL);
    struct tm* t = localtime(&now);

    fprintf(fp, "[%04d-%02d-%02d %02d:%02d:%02d] [%s] %s\n",
        t->tm_year + 1900,
        t->tm_mon + 1,
        t->tm_mday,
        t->tm_hour,
        t->tm_min,
        t->tm_sec,
        level,
        msg);

    fflush(fp);
    fclose(fp);

    // 解锁
    pthread_mutex_unlock(&log_mutex);
}

// 业务线程1：玩家逻辑
void* business_thread1(void* arg) {
    while (1) {
        write_log("THREAD1", "玩家登录业务运行中");
        sleep(2);
    }
}

// 业务线程2：场景逻辑
void* business_thread2(void* arg) {
    while (1) {
        write_log("THREAD2", "场景刷新业务运行中");
        sleep(3);
    }
}

// 业务线程3：聊天逻辑
void* business_thread3(void* arg) {
    while (1) {
        write_log("THREAD3", "聊天消息转发中");
        sleep(1);
    }
}



// 查询玩家1001是否在线（查redis）
int check_player_online() {
    redisContext* c = redisConnect("192.168.72.131", 6379);
    if (c == NULL || c->err) {
        write_log("REDIS", "连接redis失败");
        if (c) redisFree(c);
        return 0;
    }
    redisReply* reply = (redisReply*)redisCommand(c, "GET player:1001:online");
    int online = 0;
    if (reply && reply->type == REDIS_REPLY_STRING && strcmp(reply->str, "1") == 0) {
        write_log("REDIS", "玩家1001在线");
        online = 1;
    }
    freeReplyObject(reply);
    redisFree(c);
    return online;
}

// 查询mysql玩家信息，并回写redis
void query_player_from_mysql_and_update_redis() {
    MYSQL* conn = mysql_init(NULL);
    if (!mysql_real_connect(conn, "192.168.72.131", "root", "aabbcc", "game_db", 3306, NULL, 0)) {
        write_log("MYSQL", "连接mysql失败");
        mysql_close(conn);
        return;
    }
    if (mysql_query(conn, "SELECT id, Name FROM player WHERE id=1001")) {
        write_log("MYSQL", "查询player表失败");
        mysql_close(conn);
        return;
    }
    MYSQL_RES* res = mysql_store_result(conn);
    MYSQL_ROW row = mysql_fetch_row(res);
    if (row) {
        char logmsg[128];
        snprintf(logmsg, sizeof(logmsg), "MySQL查到玩家: Id=%s, Name=%s", row[0], row[1] ? row[1] : "NULL");
        write_log("MYSQL", logmsg);

        // 回写redis
        redisContext* c = redisConnect("192.168.72.131", 6379);
        if (c && !c->err) {
            redisReply* reply = (redisReply*)redisCommand(c, "SET player:1001:online 1");
            freeReplyObject(reply);
            redisFree(c);
        }
    } else {
        write_log("MYSQL", "未查到玩家1001");
    }
    mysql_free_result(res);
    mysql_close(conn);
}


// 监听线程函数
void* listen_thread(void* arg) {
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd < 0) {
        write_log("LISTEN", "创建socket失败");
        return NULL;
    }

    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in servaddr;
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(8002);

    if (bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        write_log("LISTEN", "bind失败");
        close(listenfd);
        return NULL;
    }

    if (listen(listenfd, 5) < 0) {
        write_log("LISTEN", "listen失败");
        close(listenfd);
        return NULL;
    }

    write_log("LISTEN", "监听线程已启动，端口8002");

    while (1) {
        struct sockaddr_in cliaddr;
        socklen_t len = sizeof(cliaddr);
        int connfd = accept(listenfd, (struct sockaddr*)&cliaddr, &len);
        if (connfd < 0) {
            write_log("LISTEN", "accept失败");
            continue;
        }

        char ipbuf[32];
        const char* ip = inet_ntop(AF_INET, &cliaddr.sin_addr, ipbuf, sizeof(ipbuf));
        char logmsg[128];
        snprintf(logmsg, sizeof(logmsg), "有连接到来，IP: %s, 端口: %d", ip ? ip : "未知", ntohs(cliaddr.sin_port));
        write_log("LISTEN", logmsg);
        if (check_player_online() != 1) {
            write_log("LISTEN", "玩家1001不在线,查询MySQL并更新Redis");
            query_player_from_mysql_and_update_redis();
        } else {
            write_log("LISTEN", "玩家1001已经在线,无需查询MySQL");
        }

        //close(connfd);
    }

    close(listenfd);
    return NULL;
}

// 创建监听线程的函数
void start_listen_thread() {
    pthread_t tid;
    pthread_create(&tid, NULL, listen_thread, NULL);
    pthread_detach(tid);
}

int main() {
    // 初始化日志锁
    pthread_mutex_init(&log_mutex, NULL);

    write_log("MAIN", "游戏服务器启动成功！");
    start_listen_thread();

    // // 创建3个业务线程
    // pthread_t t1, t2, t3;
    // pthread_create(&t1, NULL, business_thread1, NULL);
    // pthread_create(&t2, NULL, business_thread2, NULL);
    // pthread_create(&t3, NULL, business_thread3, NULL);

    // 主线程循环
    while (1) {
        write_log("MAIN", "主线程心跳运行中");
        sleep(1);
    }

    // // 等待线程（不会执行到）
    // pthread_join(t1, NULL);
    // pthread_join(t2, NULL);
    // pthread_join(t3, NULL);
    pthread_mutex_destroy(&log_mutex);

    return 0;
}