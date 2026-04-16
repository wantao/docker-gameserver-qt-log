#include "mainwindow.h"
#include <QVBoxLayout>
#include <QMessageBox>
#include <QDir>
#include <QProcess>
#include <QTcpSocket>
#include <QTimer>
#include <QDebug>
#include <QTextCursor>
#include <QProcessEnvironment>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , currentServerId(-1)
    , sshProcess(new QProcess(this))
    , logSocket(new QTcpSocket(this))
    , reconnectTimer(new QTimer(this))
{
    setWindowTitle("CentOS 日志实时查看工具");
    resize(1000, 700);
    initUI();

    serverPortMap[1001] = 8011;
    serverPortMap[1002] = 8012;

    reconnectTimer->setInterval(3000);
    reconnectTimer->setSingleShot(false);

    connect(logSocket, &QTcpSocket::readyRead, this, &MainWindow::onLogDataReceived);
    connect(logSocket, &QTcpSocket::connected, this, &MainWindow::onSocketConnected);
    connect(logSocket, &QTcpSocket::disconnected, this, &MainWindow::onSocketDisconnected);

    // Qt5.14 兼容错误信号
    connect(logSocket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error),
            this, &MainWindow::onSocketError);

    connect(reconnectTimer, &QTimer::timeout, this, [this]() {
        if (currentServerId != -1 && logSocket->state() != QAbstractSocket::ConnectedState) {
            int port = serverPortMap.value(currentServerId, 0);
            if (port > 0) {
                logEdit->append("重连中...");
                logSocket->connectToHost("192.168.72.131", port);
            }
        }
    });
}

MainWindow::~MainWindow()
{
    stopCurrentLogProcess();
}

void MainWindow::initUI()
{
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    cbbServer = new QComboBox(this);
    cbbServer->addItem("选择区服", -1);
    cbbServer->addItem("1001服", 1001);
    cbbServer->addItem("1002服", 1002);
    mainLayout->addWidget(cbbServer);

    logEdit = new QTextEdit(this);
    logEdit->setReadOnly(true);
    mainLayout->addWidget(logEdit);

    connect(cbbServer, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onServerChanged);
}

void MainWindow::onServerChanged(int index)
{
    Q_UNUSED(index);
    int newId = cbbServer->currentData().toInt();
    if (newId == currentServerId) return;

    stopCurrentLogProcess();
    logEdit->clear();
    currentServerId = newId;

    if (currentServerId == -1) {
        logEdit->append("已停止");
        return;
    }

    startServerLog(currentServerId);
}

void MainWindow::startServerLog(int serverId)
{
    int port = serverPortMap.value(serverId);

    logEdit->append("===== 启动日志服务（分两步执行）=====");

    // 你的 SSH 路径（已确认可用）
    QString sshPath = "ssh";

    // ==============================
    // 第一步：只建立 SSH 连接（不执行任何命令）
    // ==============================
    QStringList args;
    args << "root@192.168.72.131";

    // 清理旧连接
    disconnect(sshProcess, nullptr, this, nullptr);
    sshProcess->close();

    logEdit->append("[步骤1] 正在连接 SSH...");

    // ==============================
    // 连接成功后 → 第二步：发送执行命令
    // ==============================
    connect(sshProcess, &QProcess::started, this, [=]() {
        logEdit->append("[步骤2] SSH 连接成功！发送日志命令...");

        // 你要执行的核心命令
        QString cmd = QString("docker exec game_%1 tail -f /game/log/game.log | nc -lk 0.0.0.0 %2 &\n")
                          .arg(serverId).arg(port);

        // 发送命令到 SSH 终端
        sshProcess->write(cmd.toUtf8());

        // 发送完命令 → 延迟 1.5 秒连接端口
        QTimer::singleShot(1500, this, [=]() {
            logEdit->append("[步骤3] 连接日志端口：" + QString::number(port));
            logSocket->connectToHost("192.168.72.131", port);
            reconnectTimer->start();
        });
    });

    // 实时输出日志（关键！能收到 Docker 日志 + NC 数据）
    connect(sshProcess, &QProcess::readyReadStandardOutput, this, [=]() {
        QByteArray data = sshProcess->readAllStandardOutput();
        QString log = QString::fromUtf8(data).trimmed();
        if (!log.isEmpty()) {
            logEdit->append("[实时日志] " + log);
            logEdit->moveCursor(QTextCursor::End);
        }
    });

    // 错误输出
    connect(sshProcess, &QProcess::readyReadStandardError, this, [=]() {
        QString err = sshProcess->readAllStandardError();
        if(!err.trimmed().isEmpty())
            logEdit->append("[SSH错误] " + err);
    });

    // 启动 SSH（只连接，不执行命令）
    sshProcess->start(sshPath, args);

    if (!sshProcess->waitForStarted(4000)) {
        logEdit->append("❌ SSH 连接失败：" + sshProcess->errorString());
    }
}

void MainWindow::stopCurrentLogProcess()
{
    reconnectTimer->stop();
    logSocket->disconnectFromHost();

    // 远程停止CentOS上的残留进程
    if (currentServerId != -1) {
        int port = serverPortMap.value(currentServerId);
        QString sshPath = "C:\\Windows\\System32\\OpenSSH\\ssh.exe";
        if (!QFile::exists(sshPath)) sshPath = "ssh.exe";

        QStringList stopArgs;
        stopArgs << "root@192.168.72.131"
                 << QString("pkill -f 'tail -f /game/log/game_%1.log'; pkill -f 'nc -lk 0.0.0.0 %2'")
                    .arg(currentServerId).arg(port);

        QProcess stopProcess;
        stopProcess.start(sshPath, stopArgs);
        stopProcess.waitForFinished(3000);
    }

    if (sshProcess->state() != QProcess::NotRunning) {
        sshProcess->kill();
        sshProcess->waitForFinished(3000);
    }
    currentServerId = -1;
}

void MainWindow::onLogDataReceived()
{
    QByteArray data = logSocket->readAll();
    // 处理多行日志+粘包，过滤空行
    QString logData = QString::fromUtf8(data).trimmed();
    if (!logData.isEmpty()) {
        QStringList lines = logData.split("\n");
        for (const QString &line : lines) {
            if (!line.trimmed().isEmpty()) {
                logEdit->append(line);
            }
        }
    }
    logEdit->moveCursor(QTextCursor::End);
}

void MainWindow::onSocketConnected()
{
    logEdit->append("✅ 日志连接成功！");
}

void MainWindow::onSocketDisconnected()
{
    logEdit->append("❌ 断开连接");
}

void MainWindow::onSocketError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error);
    logEdit->append("连接异常：" + logSocket->errorString());
}
