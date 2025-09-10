QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# 包含路径设置
INCLUDEPATH += /usr/include/SDL2 \
               /snap/ffmpeg/current/usr/lib

# 链接库设置
LIBS += -L/usr/lib -lSDL2 \
        -lavformat -lavcodec -lavutil -lswscale -lswresample


# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    Audio.cpp \
    Video.cpp \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    Audio.h \
    Video.h \
    mainwindow.h

FORMS += \
    mainwindow.ui





# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    res.qrc
