QT       += core gui widgets network

TARGET = LogClient
TEMPLATE = app

SOURCES += main.cpp \
           mainwindow.cpp

HEADERS += mainwindow.h

# 彻底删除 FORMS 行，不再依赖.ui文件
# FORMS   += mainwindow.ui
