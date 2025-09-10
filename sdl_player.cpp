#include "sdl_player.h"
#include "ui_sdl_player.h"

sdl_player::sdl_player(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::sdl_player)
{
    ui->setupUi(this);
}

sdl_player::~sdl_player()
{
    delete ui;
}

void sdl_player::on_begin_clicked()
{
    if(is_first){
        connect(workerThread, &QThread::started, a, &video::doWork);
        a->moveToThread(workerThread);
        workerThread->start();
        is_first=0;
    }
    else{
        a->begin();
    }

}





void sdl_player::on_pause_clicked()
{
    a->begin();
}
