#include "videoworker.h"
#include <QDebug>

// 构造函数：初始化成员变量
VideoWorker::VideoWorker(QLabel* imgLabel, QObject* parent)
    : QObject(parent)
    , m_imgLabel(imgLabel)
    , m_isPaused(false)
{
    // 初始化FFmpeg（全局初始化，只需一次，可放在main函数中）
    av_log_set_level(AV_LOG_INFO); // 设置日志级别
}

// 析构函数：释放所有资源
VideoWorker::~VideoWorker()
{
    // 释放SDL资源（按创建逆序释放）
    if (m_sdlTexture) {
        SDL_DestroyTexture(m_sdlTexture);
        m_sdlTexture = nullptr;
    }
    if (m_sdlRenderer) {
        SDL_DestroyRenderer(m_sdlRenderer);
        m_sdlRenderer = nullptr;
    }
    if (m_sdlWindow) {
        SDL_DestroyWindow(m_sdlWindow);
        m_sdlWindow = nullptr;
    }
    SDL_QuitSubSystem(SDL_INIT_VIDEO); // 退出SDL视频子系统

    // 释放FFmpeg资源
    if (m_swsCtx) {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
    }
    if (m_pFrame) {
        av_frame_free(&m_pFrame);
        m_pFrame = nullptr;
    }
    if (m_pCodecCtx) {
        avcodec_free_context(&m_pCodecCtx);
        m_pCodecCtx = nullptr;
    }
    if (m_pFormatCtx) {
        avformat_close_input(&m_pFormatCtx);
        m_pFormatCtx = nullptr;
    }
}

// 检查暂停状态（子线程内调用，阻塞直到继续）
void VideoWorker::checkPause()
{
    QMutexLocker locker(&m_mutex); // 自动加锁/解锁
    while (m_isPaused) {
        // 暂停时阻塞，释放锁并等待唤醒
        m_cond.wait(&m_mutex);
    }
}

// 初始化FFmpeg：打开文件、查找流、初始化解码器
bool VideoWorker::initFFmpeg(const char* filePath)
{
    // 1. 打开输入文件
    if (avformat_open_input(&m_pFormatCtx, filePath, nullptr, nullptr) < 0) {
        emit renderError("无法打开视频文件");
        return false;
    }

    // 2. 获取流信息
    if (avformat_find_stream_info(m_pFormatCtx, nullptr) < 0) {
        emit renderError("无法获取流信息");
        avformat_close_input(&m_pFormatCtx);
        return false;
    }

    // 3. 查找视频流
    for (unsigned int i = 0; i < m_pFormatCtx->nb_streams; ++i) {
        if (m_pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            m_videoIndex = i;
            break;
        }
    }
    if (m_videoIndex == -1) {
        emit renderError("未找到视频流");
        avformat_close_input(&m_pFormatCtx);
        return false;
    }

    // 4. 初始化解码器
    AVCodecParameters* codecPar = m_pFormatCtx->streams[m_videoIndex]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(codecPar->codec_id);
    if (!codec) {
        emit renderError("未找到对应解码器");
        avformat_close_input(&m_pFormatCtx);
        return false;
    }

    // 5. 分配解码器上下文并复制参数
    m_pCodecCtx = avcodec_alloc_context3(codec);
    if (!m_pCodecCtx) {
        emit renderError("无法分配解码器上下文");
        avformat_close_input(&m_pFormatCtx);
        return false;
    }
    if (avcodec_parameters_to_context(m_pCodecCtx, codecPar) < 0) {
        emit renderError("无法复制解码器参数");
        avcodec_free_context(&m_pCodecCtx);
        avformat_close_input(&m_pFormatCtx);
        return false;
    }

    // 6. 打开解码器
    if (avcodec_open2(m_pCodecCtx, codec, nullptr) < 0) {
        emit renderError("无法打开解码器");
        avcodec_free_context(&m_pCodecCtx);
        avformat_close_input(&m_pFormatCtx);
        return false;
    }

    // 7. 初始化帧存储
    m_pFrame = av_frame_alloc();
    if (!m_pFrame) {
        emit renderError("无法分配帧内存");
        avcodec_close(m_pCodecCtx);
        avcodec_free_context(&m_pCodecCtx);
        avformat_close_input(&m_pFormatCtx);
        return false;
    }

    // 8. 初始化格式转换上下文（源尺寸->目标尺寸，源像素格式->YUV420P）
    m_swsCtx = sws_getContext(
        m_pCodecCtx->width,        // 源宽度
        m_pCodecCtx->height,       // 源高度
        m_pCodecCtx->pix_fmt,      // 源像素格式
        m_imgLabel->width(),       // 目标宽度（QLabel宽度）
        m_imgLabel->height(),      // 目标高度（QLabel高度）
        AV_PIX_FMT_YUV420P,        // 目标像素格式（SDL支持的IYUV）
        SWS_BICUBIC,               // 缩放算法（双立方，画质较好）
        nullptr, nullptr, nullptr
        );
    if (!m_swsCtx) {
        emit renderError("无法初始化格式转换上下文");
        av_frame_free(&m_pFrame);
        avcodec_close(m_pCodecCtx);
        avcodec_free_context(&m_pCodecCtx);
        avformat_close_input(&m_pFormatCtx);
        return false;
    }

    // 9. 创建SDL纹理（用于渲染YUV数据）
    m_sdlTexture = SDL_CreateTexture(
        m_sdlRenderer,
        SDL_PIXELFORMAT_IYUV,      // 对应YUV420P
        SDL_TEXTUREACCESS_STREAMING,
        m_pCodecCtx->width,
        m_pCodecCtx->height
        );
    if (!m_sdlTexture) {
        emit renderError(QString("无法创建SDL纹理: %1").arg(SDL_GetError()));
        sws_freeContext(m_swsCtx);
        av_frame_free(&m_pFrame);
        avcodec_close(m_pCodecCtx);
        avcodec_free_context(&m_pCodecCtx);
        avformat_close_input(&m_pFormatCtx);
        return false;
    }

    return true;
}

// 开始播放：初始化SDL并进入解码循环
void VideoWorker::startPlay(const QString& filePath)
{
    // 1. 初始化SDL视频子系统
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
        emit renderError(QString("SDL初始化失败: %1").arg(SDL_GetError()));
        return;
    }

    // 2. 从Qt的QLabel创建SDL窗口（绑定窗口句柄）
    m_sdlWindow = SDL_CreateWindowFrom((void*)m_imgLabel->winId());
    if (!m_sdlWindow) {
        emit renderError(QString("无法创建SDL窗口: %1").arg(SDL_GetError()));
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return;
    }

    // 3. 创建SDL渲染器（硬件加速）
    m_sdlRenderer = SDL_CreateRenderer(
        m_sdlWindow,
        -1,                      // 自动选择渲染驱动
        SDL_RENDERER_ACCELERATED // 硬件加速
        );
    if (!m_sdlRenderer) {
        emit renderError(QString("无法创建SDL渲染器: %1").arg(SDL_GetError()));
        SDL_DestroyWindow(m_sdlWindow);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return;
    }

    // 4. 初始化FFmpeg并开始解码渲染
    if (!initFFmpeg(filePath.toUtf8().constData())) {
        // 初始化失败，清理SDL资源
        SDL_DestroyRenderer(m_sdlRenderer);
        SDL_DestroyWindow(m_sdlWindow);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return;
    }

    // 5. 进入解码渲染循环
    decodeAndRenderLoop();

    // 6. 播放结束，发送信号
    emit playFinished();
}

// 暂停播放
void VideoWorker::pause()
{
    QMutexLocker locker(&m_mutex);
    m_isPaused = true;
}

// 继续播放
void VideoWorker::resume()
{
    QMutexLocker locker(&m_mutex);
    m_isPaused = false;
    m_cond.wakeOne(); // 唤醒等待的子线程
}

// 解码并渲染循环：核心逻辑
void VideoWorker::decodeAndRenderLoop()
{
    AVPacket* packet = av_packet_alloc(); // 存储未解码的数据包
    AVFrame* frameYUV = av_frame_alloc(); // 存储转换后的YUV帧

    // 为YUV帧分配内存（目标尺寸：QLabel的宽高）
    int bufferSize = av_image_get_buffer_size(
        AV_PIX_FMT_YUV420P,
        m_imgLabel->width(),
        m_imgLabel->height(),
        1 // 对齐方式
        );
    uint8_t* outBuffer = (uint8_t*)av_malloc(bufferSize);
    av_image_fill_arrays(
        frameYUV->data,
        frameYUV->linesize,
        outBuffer,
        AV_PIX_FMT_YUV420P,
        m_imgLabel->width(),
        m_imgLabel->height(),
        1
        );

    // 渲染区域（填满QLabel）
    SDL_Rect renderRect = {
        0, 0,
        m_imgLabel->width(),
        m_imgLabel->height()
    };

    // 循环读取并解码视频帧
    while (av_read_frame(m_pFormatCtx, packet) >= 0) {
        checkPause(); // 检查是否需要暂停

        if (packet->stream_index != m_videoIndex) {
            // 非视频流，直接释放
            av_packet_unref(packet);
            continue;
        }

        // 发送数据包到解码器
        int sendRet = avcodec_send_packet(m_pCodecCtx, packet);
        if (sendRet < 0) {
            av_log(nullptr, AV_LOG_ERROR, "发送数据包失败\n");
            av_packet_unref(packet);
            break;
        }

        // 循环接收解码后的帧（处理B帧可能返回多帧）
        while (true) {
            int recvRet = avcodec_receive_frame(m_pCodecCtx, m_pFrame);
            if (recvRet == AVERROR(EAGAIN)) {
                // 需要更多数据包，退出当前循环
                break;
            } else if (recvRet == AVERROR_EOF) {
                // 解码完成
                av_log(nullptr, AV_LOG_INFO, "视频解码完成\n");
                goto endLoop; // 跳转到循环外清理
            } else if (recvRet < 0) {
                av_log(nullptr, AV_LOG_ERROR, "接收帧失败\n");
                goto endLoop;
            }

            // 格式转换：源帧 -> YUV420P（目标尺寸）
            sws_scale(
                m_swsCtx,
                (const uint8_t* const*)m_pFrame->data, // 源数据
                m_pFrame->linesize,                    // 源行间距
                0,                                     // 起始行
                m_pCodecCtx->height,                    // 源高度
                frameYUV->data,                        // 目标数据
                frameYUV->linesize                     // 目标行间距
                );

            // 更新SDL纹理（YUV数据）
            SDL_UpdateYUVTexture(
                m_sdlTexture,
                nullptr, // 更新整个纹理
                frameYUV->data[0], frameYUV->linesize[0], // Y分量
                frameYUV->data[1], frameYUV->linesize[1], // U分量
                frameYUV->data[2], frameYUV->linesize[2]  // V分量
                );

            // 渲染到屏幕
            SDL_RenderClear(m_sdlRenderer);                // 清空渲染目标
            SDL_RenderCopy(m_sdlRenderer, m_sdlTexture, nullptr, &renderRect); // 复制纹理到渲染目标
            SDL_RenderPresent(m_sdlRenderer);              // 显示渲染结果

            // 控制播放速度（约25fps）
            SDL_Delay(40);
        }

        av_packet_unref(packet); // 释放数据包（复用）
    }

endLoop:
    // 释放局部资源
    av_packet_free(&packet);
    av_frame_free(&frameYUV);
    av_free(outBuffer); // 释放YUV帧的缓冲区
}
