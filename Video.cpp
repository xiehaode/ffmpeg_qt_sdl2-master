#include "Video.h"

video::video(QString f,QObject* parent)
    : QObject(parent)
    , ispaused(false),f(f)  // 初始化为未暂停
{

}

void video::doWork()
{

    AVFormatContext    *pFormatCtx;
    int                i, videoindex;
    AVCodecParameters *pCodecParameters;
    AVCodecContext* pCodecCtx;
    const AVCodec            *pCodec;
    AVFrame    *pFrame, *pFrameYUV;
    unsigned char *out_buffer;
    AVPacket *packet;
    int ret, got_picture;
    struct SwsContext *img_convert_ctx;
    f.toStdString();
    char filepath[] = "1.mp4";
    //初始化编解码库
    //已经无需使用的函数？
    //avformat_network_init();
    //创建AVFormatContext对象，与码流相关的结构。
    pFormatCtx = avformat_alloc_context();
    //初始化pFormatCtx结构
    if (avformat_open_input(&pFormatCtx, filepath, NULL, NULL) != 0){
        printf("Couldn't open input stream.\n");
        return ;
    }
    //获取音视频流数据信息
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0){
        printf("Couldn't find stream information.\n");
        return ;
    }
    videoindex = -1;
    //nb_streams视音频流的个数，这里当查找到视频流时就中断了。
    for (i = 0; i < pFormatCtx->nb_streams; i++)
        if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO){
            videoindex = i;
            break;
        }
    if (videoindex == -1){
        printf("Didn't find a video stream.\n");
        return ;
    }
    //获取视频流编码结构AVCodecParameters *pCodecParameters
    pCodecParameters = pFormatCtx->streams[videoindex]->codecpar;
    //查找解码器
    pCodec = avcodec_find_decoder(pCodecParameters->codec_id);
    if (pCodec == NULL){
        printf("Codec not found.\n");
        return ;
    }
    pCodecCtx = avcodec_alloc_context3(pCodec);
    if (avcodec_parameters_to_context(pCodecCtx, pCodecParameters) < 0) {
        // 参数复制失败
        avcodec_free_context(&pCodecCtx);

    }
    //用于初始化pCodecCtx结构
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0){
        printf("Could not open codec.\n");
        return ;
    }
    //创建帧结构，此函数仅分配基本结构空间，图像数据空间需通过av_malloc分配
    pFrame = av_frame_alloc();
    pFrameYUV = av_frame_alloc();
    //创建动态内存,创建存储图像数据的空间
    //av_image_get_buffer_size获取一帧图像需要的大小
    out_buffer = (unsigned char *)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_YUV420P, 800, 600, 1));
    av_image_fill_arrays(pFrameYUV->data, pFrameYUV->linesize, out_buffer,
                         AV_PIX_FMT_YUV420P, 800, 600, 1);

    packet = (AVPacket *)av_malloc(sizeof(AVPacket));
    //Output Info-----------------------------
    printf("--------------- File Information ----------------\n");
    //此函数打印输入或输出的详细信息
    av_dump_format(pFormatCtx, 0, filepath, 0);
    printf("-------------------------------------------------\n");
    //初始化img_convert_ctx结构
    img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt,
                                     800, 600, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);
    //SDL---------------------------
    SDL_Window *screen;
    SDL_Renderer* sdlRenderer;
    SDL_Texture* sdlTexture;
    SDL_Rect sdlRect;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        printf("Could not initialize SDL - %s\n", SDL_GetError());
        return;
    }
    screen = SDL_CreateWindow("MyPlayer",800,600,800,600,SDL_WINDOW_SHOWN);
    //screen = SDL_CreateWindowFrom((void *)this->imgLabel->winId());
    if(screen==NULL)
    {
        printf("Could not create window - %s\n", SDL_GetError());
        return;
    }
    sdlRenderer = SDL_CreateRenderer(screen, -1, 0);
    sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, pCodecCtx->width, pCodecCtx->height);
    sdlRect.x = 0;
    sdlRect.y = 0;
    sdlRect.w = pCodecCtx->width;
    sdlRect.h = pCodecCtx->height;
    //end SDL-----------------------
    //av_read_frame读取一帧未解码的数据
    SDL_Event event;
    bool quit = false;//窗口结束条件
    while (av_read_frame(pFormatCtx, packet) >= 0 || !quit) {
        SDL_PollEvent(&event);
        switch (event.type) {
        case SDL_QUIT://输入事件冲菜单退出(其实就是点击右上角的叉号的时候会执行这个)
            //SDL_Log("quit fair");
            quit = true;
            break;
        }
        if(quit){
            break;
        }
        //检查停止
        checkPause();
        // 只处理视频流
        if (packet->stream_index == videoindex) {
            //  发送数据包到解码器
            int send_ret = avcodec_send_packet(pCodecCtx, packet);
            if (send_ret < 0) {
                //av_log(nullptr, AV_LOG_ERROR, "发送数据包失败: %s\n", av_err2str(send_ret));
                break; // 发送失败，退出循环
            }

            //  循环接收解码后的帧（可能一次发送对应多帧输出，如 B 帧）
            while (true) {
                // 接收解码后的帧
                int recv_ret = avcodec_receive_frame(pCodecCtx, pFrame);

                // 处理接收结果
                if (recv_ret == AVERROR(EAGAIN)) {
                    // 需要更多数据包才能继续解码，跳出当前循环，等待下一个 packet
                    break;
                } else if (recv_ret == AVERROR_EOF) {
                    // 解码已完成，退出所有循环
                    av_log(nullptr, AV_LOG_INFO, "视频解码完成\n");
                    goto end_decode; // 跳转到循环外释放资源
                } else if (recv_ret < 0) {
                    // 发生错误（如解码失败）
                    //av_log(nullptr, AV_LOG_ERROR, "接收帧失败: %s\n", av_err2str(recv_ret));
                    goto end_decode;
                }

                // 成功解码一帧，处理并渲染
                //  格式转换（YUV420P）
                sws_scale(img_convert_ctx,
                          (const unsigned char* const*)pFrame->data,
                          pFrame->linesize,
                          0,
                          pCodecCtx->height,
                          pFrameYUV->data,
                          pFrameYUV->linesize);

                // SDL 渲染
                SDL_UpdateYUVTexture(sdlTexture, &sdlRect,
                                     pFrameYUV->data[0], pFrameYUV->linesize[0],
                                     pFrameYUV->data[1], pFrameYUV->linesize[1],
                                     pFrameYUV->data[2], pFrameYUV->linesize[2]);
                SDL_RenderClear(sdlRenderer);
                SDL_RenderCopy(sdlRenderer, sdlTexture, nullptr, &sdlRect);
                SDL_RenderPresent(sdlRenderer);

                //  控制播放速度（约 25fps）1000/40
                SDL_Delay(40);
            }
        }

    }
end_decode:
    SDL_DestroyTexture(sdlTexture);
    SDL_DestroyRenderer(sdlRenderer);
    SDL_DestroyWindow(screen);
    // 4. 释放当前 packet 的数据（复用 packet 结构体）
    av_packet_unref(packet);
    sws_freeContext(img_convert_ctx);
    av_frame_free(&pFrameYUV);
    av_frame_free(&pFrame);
    avcodec_close(pCodecCtx);
    avformat_close_input(&pFormatCtx);
}

void video::pause()
{
    QMutexLocker locker(&pauseMutex);
    ispaused = true;
}




void video::begin()
{
    if(is_first){
        is_first=0;
        doWork();
        return;
    }
    QMutexLocker locker(&pauseMutex);
    ispaused = false;
}

void video::checkPause()
{
    QMutexLocker locker(&pauseMutex);
    while (ispaused) {
        pauseCond.wait(&pauseMutex); // 暂停时等待
    }
}
