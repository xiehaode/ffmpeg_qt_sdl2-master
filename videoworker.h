#ifndef VIDEOWORKER_H
#define VIDEOWORKER_H

#include <QObject>
#include <QMutex>
#include <QWaitCondition>
#include <QLabel>

// FFmpeg和SDL头文件
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/avutil.h>
#include <SDL2/SDL.h>
}

class VideoWorker : public QObject {
    Q_OBJECT
public:
    explicit VideoWorker(QLabel* imgLabel, QObject* parent = nullptr);
    ~VideoWorker() override;

signals:
    // 错误信息通知（主线程处理UI显示）
    void renderError(const QString& error);
    // 播放结束通知
    void playFinished();

public slots:
    // 开始播放（接收视频文件路径）
    void startPlay(const QString& filePath);
    // 暂停播放
    void pause();
    // 继续播放
    void resume();

private:
    // 检查暂停状态（子线程内调用，可能阻塞）
    void checkPause();
    // 初始化FFmpeg（返回是否成功）
    bool initFFmpeg(const char* filePath);
    // 解码并渲染循环
    void decodeAndRenderLoop();

private:
    // Qt UI组件（仅用于获取窗口句柄，不直接操作）
    QLabel* m_imgLabel;

    // 线程同步变量
    QMutex m_mutex;               // 保护暂停状态的互斥锁
    QWaitCondition m_cond;        // 暂停等待的条件变量
    bool m_isPaused;              // 暂停状态标志

    // SDL资源（子线程内创建和使用）
    SDL_Window* m_sdlWindow = nullptr;    // SDL窗口（绑定Qt的QLabel）
    SDL_Renderer* m_sdlRenderer = nullptr;// SDL渲染器
    SDL_Texture* m_sdlTexture = nullptr;  // SDL纹理（用于YUV渲染）

    // FFmpeg资源
    AVFormatContext* m_pFormatCtx = nullptr;  // 格式上下文
    AVCodecContext* m_pCodecCtx = nullptr;    // 解码器上下文
    AVFrame* m_pFrame = nullptr;              // 解码后的原始帧
    SwsContext* m_swsCtx = nullptr;           // 格式转换上下文（缩放+像素格式）
    int m_videoIndex = -1;                    // 视频流索引
};

#endif // VIDEOWORKER_H
