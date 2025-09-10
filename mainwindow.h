#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QWaitCondition>
#include <QMutexLocker>
#include <QLabel>
#include <QMainWindow>
#include <QThread>
#include <QDebug>
//#include <SDL2/SDL.h>
#include "Video.h"
#include "Audio.h"
#include <QFileDialog>
extern "C"{

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void checkPause();

    QThread *videoThread;
    QThread *audioThread;

    SDL_Renderer *render;
    SDL_Window *window;
    QLabel *imgLabel;
    QMutex pauseMutex;
    QWaitCondition pauseCond;
    bool is_first=1;

    QString aFile;
    video *a;
    Audio* b;
private slots:
    void on_openfile_clicked();

signals:


private:

    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
