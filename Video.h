#ifndef VIDEO_H
#define VIDEO_H

#include <QWaitCondition>
#include <QMutexLocker>
#include <QLabel>
#include <QMainWindow>
#include <QThread>
#include <QDebug>
#include <SDL2/SDL.h>
#include <QObject>
#include <QObject>
#include <QLabel>
#include <QMutex>
#include <QWaitCondition>
#include <queue>
#include <SDL2/SDL.h>
#include <QFile>

extern "C"{
#include <libavutil/log.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}


class video : public QObject
{
    Q_OBJECT
public:
    explicit video(QString a,QObject *parent = 0);
    void doWork();
    void checkPause();

    SDL_Renderer *render;
    SDL_Window *window;
    QLabel *imgLabel;
    QMutex pauseMutex;
    QWaitCondition pauseCond;
    QString f;
    bool ispaused=0;
    bool is_first=1;
signals:
    //void resultReady(const int result);
public slots:
    void pause();
    void begin();
};

#endif // VIDEO_H
