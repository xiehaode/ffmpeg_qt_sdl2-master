#include "mainwindow.h"
#include "ui_mainwindow.h"


extern "C"
{
#include <libavutil/log.h>
#include <libavcodec/avcodec.h>
};
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{

    ui->setupUi(this);

    QIcon icon;
    icon.addFile(tr("images/001.GIF"));
    ui->openfile->setIcon(icon);

    videoThread = new QThread();
    audioThread = new QThread();


    a=new video(aFile);b=new Audio(aFile);
    a->moveToThread(videoThread);b->moveToThread(audioThread);
    connect(ui->pause, &QPushButton::clicked,a,&video::pause);
    //connect(this,&MainWindow::ui->pause->click,a,&video::pause);
    connect(ui->begin,&QPushButton::clicked,a,&video::doWork);
    //connect(workerThread, &QThread::started, a, &video::doWork);
    connect(ui->pause, &QPushButton::clicked,b,&Audio::pause);

    connect(ui->begin, &QPushButton::clicked,b,&Audio::dowork);
    videoThread->start();
    audioThread->start();

}

MainWindow::~MainWindow()
{
    videoThread->quit();
    videoThread->wait();
    delete videoThread;
    delete a;  // 删除video对象
    audioThread->quit();
    audioThread->wait();
    delete audioThread;
    delete b;  //删除Audio对象
    delete ui;
}


void MainWindow::on_openfile_clicked()
{
    QString curPath=QDir::homePath();//获取系统当前目录
    QString dlgTitle="选择视频文件"; //对话框标题
    QString filter="wmv文件(*.wmv);;mp4文件(*.mp4);;所有文件(*.*)"; //文件过滤器
    aFile=QFileDialog::getOpenFileName(this,dlgTitle,curPath,filter);
}

