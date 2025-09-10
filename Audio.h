#ifndef AUDIO_H
#define AUDIO_H


#include <QObject>
#include <cstdio>
#include <string>
#include <fstream>
#include <vector>
#include <QMutex>
#include <QWaitCondition>
#include <QFile>

#define SDL_MAIN_HANDLED
#include "SDL.h"

extern "C" {
#include "libswresample/swresample.h"
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

class Audio : public QObject
{
    Q_OBJECT
public:
    explicit Audio(QString a,QObject *parent = nullptr);
    AVFormatContext *OpenAudioFromFile(const std::string &file_path);
    AVFormatContext *OpenAudioFromData(const std::string &data);
    QMutex pauseMutex;
    QWaitCondition pauseCond;
    bool ispaused=0;
    bool is_first=1;
    QString f;
    void dowork();
    void checkPause();
signals:
public slots:
    void pause();
    void begin();
};

#endif // AUDIO_H
