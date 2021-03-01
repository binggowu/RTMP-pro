#ifndef PULLWORK_H
#define PULLWORK_H

#include "mediabase.h"

namespace LQF
{
    class RTMPPlayer;
    class AVSync;
    class AudioDecodeLoop;
    class AudioResampler;
    class AudioOutSDL;
    class VideoDecodeLoop;
    class ImageScaler;
    class VideoOutputLoop;
    class VideoOutSDL;

    class PullWork
    {
    public:
        PullWork();
        ~PullWork();

        RET_CODE Init(const Properties &properties);

        // Audio编码后的数据回调
        void audioInfoCallback(int what, MsgBaseObj *data, bool flush = false);
        // Video编码后的数据回调
        void videoInfoCallback(int what, MsgBaseObj *data, bool flush = false);

        // AudioDecoderLoop::Post(), 入AudioDecoderLoop的PacketQueue
        void audioDataCallback(void *);
        // VideoDecodeLoop::Post(), 入VideoDecodeLoop的PacketQueue
        void videoDataCallback(void *);

        // 重采样 + SDL输出
        void pcmFrameCallback(void *);
        void yuvFrameCallback(void *frame);

        void displayVideo(uint8_t *yuv, uint32_t size, int32_t format);

        int avSyncCallback(int64_t pts, int32_t duration, int64_t &get_diff);

    private:
        std::string rtmp_url_;
        RTMPPlayer *rtmp_player_ = NULL;
        AVSync *avsync_ = NULL; // 音视频同步

        AudioDecodeLoop *audio_decode_loop_ = NULL; // video解码线程
        AudioResampler *audio_resampler_ = NULL;    // 重采样, 将解码后的数据设置成输出需要音频
        AudioOutSDL *audio_out_sdl_ = NULL;         // audio的SDL输出

        VideoDecodeLoop *video_decode_loop_; // video解码线程
        ImageScaler *img_scale_ = NULL;      // 视频尺寸变换
        VideoOutputLoop *video_output_loop_ = NULL;
        VideoOutSDL *video_out_sdl_ = NULL; // video的SDL输出

        int audio_out_sample_rate_ = 44100;
        int audio_out_sample_channels_ = 2;
        AVSampleFormat audio_out_sample_format_ = AV_SAMPLE_FMT_S16;

        int video_out_width_ = 480;
        int video_out_height_ = 270;
        AVPixelFormat video_out_format_ = AV_PIX_FMT_YUV420P;

        // 缓存控制
        int cache_duration_ = 500;
    };
}
#endif // PULLWORK_H
