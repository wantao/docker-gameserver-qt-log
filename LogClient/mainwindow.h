#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QComboBox>
#include <QTextEdit>
#include <QProcess>
#include <QTcpSocket>
#include <QTimer>
#include <QMap>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onServerChanged(int index);
    void onLogDataReceived();
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError error);

private:
    void initUI();
    void startServerLog(int serverId);
    void stopCurrentLogProcess();

    QComboBox *cbbServer;
    QTextEdit *logEdit;
    int currentServerId;
    QMap<int, int> serverPortMap;
    QProcess *sshProcess;
    QTcpSocket *logSocket;
    QTimer *reconnectTimer;
};

#endif
