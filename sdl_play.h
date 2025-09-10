#ifndef SDL_PLAY_H
#define SDL_PLAY_H

#include <QWaitCondition>
#include <QMutexLocker>
#include <QLabel>
#include <QMainWindow>
#include <QThread>
#include <QDebug>
#include <SDL2/SDL.h>
#include "Video.h"

extern "C"{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}
#include <QWidget>

class SDL_play : public QWidget
{
    Q_OBJECT
public:
    explicit SDL_play(QWidget *parent = nullptr);
    void checkPause();
    QThread* workerThread;
    SDL_Renderer *render;
    SDL_Window *window;
    QLabel *imgLabel;
    QMutex pauseMutex;
    QWaitCondition pauseCond;
    bool ispaused=0;
    bool is_first=1;

    video *a;
signals:
private slots:
    void on_begin_clicked();

    void on_pause_clicked();
};

#endif // SDL_PLAY_H
