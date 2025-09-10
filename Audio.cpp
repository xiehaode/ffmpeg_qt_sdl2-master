#include "Audio.h"

Audio::Audio(QString f,QObject *parent)
    : QObject{parent},f(f)
{

}

AVFormatContext *Audio::OpenAudioFromFile(const std::string &file_path) {
    AVFormatContext *format_ctx = avformat_alloc_context();
    if (int ret = avformat_open_input(&format_ctx, file_path.c_str(), NULL, NULL);
        ret < 0) {
        printf("avformat_open_input failed, ret: %d, path: %s\n", ret, file_path.c_str());
    }
    return format_ctx;
}

AVFormatContext *Audio::OpenAudioFromData(const std::string &data) {
    AVIOContext *avio_ctx = avio_alloc_context(
        (uint8_t *)data.data(), data.size(), 0, NULL, NULL, NULL, NULL);
    if (!avio_ctx) {
        printf("avio_alloc_context failed\n");
        return nullptr;
    }

    AVFormatContext *format_ctx = avformat_alloc_context();
    if (!format_ctx) {
        printf("avformat_alloc_context faield\n");
        return nullptr;
    }
    format_ctx->pb = avio_ctx;

    if (int ret = avformat_open_input(&format_ctx, NULL, NULL, NULL); ret < 0) {
        printf("avformat_open_input failed, ret: %d\n", ret);
        return nullptr;
    }
    return format_ctx;
}

std::string readFile(const std::string &file_path) {
    std::ifstream file(file_path);
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

void Audio::dowork() {
    printf("ffmpeg version: %s\n", av_version_info());

    SDL_version version;
    SDL_GetVersion(&version);
    printf("SDL version: %d.%d.%d\n", version.major, version.minor, version.patch);

    char *file = "1.mp4";
    AVFormatContext *pFormatCtx = NULL; //for opening multi-media file

    int audioStream = -1;

    AVCodecParameters *pCodecParameters = NULL; //codec context
    AVCodecContext *pCodecCtx = NULL;

    const AVCodec *pCodec = NULL; // the codecer
    AVFrame *pFrame = NULL;
    AVPacket *packet;

    int64_t in_channel_layout;
    struct SwrContext *au_convert_ctx;

    pFormatCtx = OpenAudioFromFile(file);
    //    std::string data = readFile(file);
    //    pFormatCtx = OpenAudioFromData(data);
    if (!pFormatCtx) {

    }

    if (avformat_find_stream_info(pFormatCtx, nullptr) < 0) {

    }

    audioStream = av_find_best_stream(pFormatCtx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    printf("av_find_best_stream %d\n", audioStream);

    if (audioStream == -1) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Din't find a video stream!");
       // Didn't find a video stream
    }

    // Get a pointer to the codec context for the video stream
    pCodecParameters = pFormatCtx->streams[audioStream]->codecpar;

    // Find the decoder for the video stream
    pCodec = avcodec_find_decoder(pCodecParameters->codec_id);
    if (pCodec == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unsupported codec!\n");
         // Codec not found
    }

    // Copy context
    pCodecCtx = avcodec_alloc_context3(pCodec);
    if (avcodec_parameters_to_context(pCodecCtx, pCodecParameters) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't copy codec context");
        ;// Error copying codec context
    }

    // Open codec
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to open decoder!\n");
        ; // Could not open codec
    }
    packet = (AVPacket *) av_malloc(sizeof(AVPacket));
    av_init_packet(packet);
    pFrame = av_frame_alloc();

    uint64_t out_channel_layout = AV_CH_LAYOUT_STEREO; //输出声道
    enum AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16; //输出格式S16
    int out_sample_rate = 44100;
    int out_channels = av_get_channel_layout_nb_channels(out_channel_layout);

    //Init
    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
        printf("Could not initialize SDL - %s\n", SDL_GetError());
        ;
    }

    // 获取可用扬声器列表，不需要可以忽略
    int deviceCount = SDL_GetNumAudioDevices(0);
    printf("SDL_GetNumAudioDevices %d\n", deviceCount);
    for (int i = 0; i < deviceCount; i++) {
        printf("Audio Device %d: %s\n", i, SDL_GetAudioDeviceName(i, 0));
    }

    SDL_AudioSpec spec;
    spec.freq = out_sample_rate;
    spec.format = AUDIO_S16SYS;
    spec.channels = out_channels;
    spec.silence = 0;
    spec.samples = 1024;
    spec.callback = nullptr;

    // 指定扬声器，不需要第一个参数可以填nullptr
    SDL_AudioDeviceID device_id = SDL_OpenAudioDevice(SDL_GetAudioDeviceName(1, 0), false, &spec, nullptr, false);
    printf("device_id %d\n", device_id);
    if (device_id == 0) {
        printf("can't open audio.\n");
        ;
    }

    in_channel_layout = av_get_default_channel_layout(pCodecCtx->channels);
    au_convert_ctx = swr_alloc_set_opts(nullptr, out_channel_layout, out_sample_fmt, out_sample_rate,
                                        in_channel_layout, pCodecCtx->sample_fmt, pCodecCtx->sample_rate, 0, NULL);
    swr_init(au_convert_ctx);

    SDL_PauseAudioDevice(device_id, 0);

    fflush(stdout);

    std::vector<uint8_t> out_buffer;
    while (av_read_frame(pFormatCtx, packet) >= 0) {
        checkPause();
        if (packet->stream_index == audioStream) {
            avcodec_send_packet(pCodecCtx, packet);
            while (avcodec_receive_frame(pCodecCtx, pFrame) == 0) {
                int out_nb_samples = swr_get_out_samples(au_convert_ctx, pFrame->nb_samples);
                out_buffer.resize(av_samples_get_buffer_size(NULL, pCodecCtx->channels, out_nb_samples, pCodecCtx->sample_fmt, 1));
                uint8_t *out = out_buffer.data();

                int ret = swr_convert(au_convert_ctx, &out, out_nb_samples, (const uint8_t **) pFrame->data,
                                      pFrame->nb_samples);
                if (ret < 0) {
                    printf("swr_convert failed %d\n", ret);
                }
                int out_samples = ret;
                SDL_QueueAudio(device_id, out, out_samples * spec.channels * av_get_bytes_per_sample(out_sample_fmt));
            }
        }
        av_packet_unref(packet);
    }
    end_decode:
    SDL_Delay(200000); // 等音频播完
    swr_free(&au_convert_ctx);
    SDL_CloseAudioDevice(device_id);
    SDL_Quit();
}

void Audio::pause()
{
    QMutexLocker locker(&pauseMutex);
    ispaused = true;
}




void Audio::begin()
{
    if(is_first){
        is_first=0;
        dowork();
        return;
    }
    QMutexLocker locker(&pauseMutex);
    ispaused = false;
}

void Audio::checkPause()
{
    QMutexLocker locker(&pauseMutex);
    while (ispaused) {
        pauseCond.wait(&pauseMutex); // 暂停时等待
    }
}
