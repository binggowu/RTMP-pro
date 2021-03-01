#ifndef PUSHWORK_H
#define PUSHWORK_H

#include "mediabase.h"

namespace LQF
{
    class RTMPPusher;
    class VideoCapturer;
    class H264Encoder;
    class VideoOutSDL;
    class AudioCapturer;
    class AudioResampler;
    // class AACEncoder;

    class PushWork
    {

    public:
        PushWork();
        ~PushWork();

        /**
     * @brief 推流初始化, 只支持H264+AAC格式
     * @param properties
     *      音频test模式:
     *          "audio_test": 缺省为0, 为1时数据读取本地文件进行播放
     *          "input_pcm_name": 测试模式时读取的文件路径
     *      麦克风采样属性:
     *          "mic_sample_fmt", 麦克风采样格式  AVSampleFormat对应的值, 缺省为AV_SAMPLE_FMT_S16
     *          "mic_sample_rate", 麦克风采样率, 缺省为480000
     *          "mic_channels", 麦克风采样通道, 缺省为2通道
     *      音频编码属性:
     *          "audio_sample_rate", 采样率, 缺省和麦克风采样率一致
     *          "audio_bitrate"   , 编码比特率, 缺省为128*1024bps
     *          "audio_channels"  , 编码通道, 缺省为2通道
     *      视频test模式:
     *          "test_test": 缺省为0, 为1时数据读取本地文件进行播放
     *          "input_yuv_name": 测试模式时读取的文件路径
     *      桌面录制属性:
     *          "desktop_x", x起始位置, 缺省为0
     *          "desktop_y", y起始位置, 缺省为0
     *          "desktop_width", 宽度, 缺省为屏幕宽带
     *          "desktop_height", 高度, 缺省为屏幕高度
     *          "desktop_pixel_format", 像素格式, AVPixelFormat对应的值, 缺省为AV_PIX_FMT_YUV420P
     *          "desktop_fps", 帧数, 缺省为25
     *      视频编码属性:
     *          "video_width", 视频宽度, 缺省值和desktop_width一致
     *          "video_height", 视频高度, 缺省值和desktop_height一致
     *          "video_fps",    帧率,, 缺省值和desktop_fps一致
     *          "video_gop",  i帧间隔, 默认和帧率一致
     *          "video_bitrate", 视频bitrate, 可以自己设置, 缺省值则根据不同分辨率自动进行调整
     *          "video_b_frames", 连续B帧最大数量, 缺省为0, 直播一般都不回用设置b帧
     *      rtmp推流:
     *          "rtmp_url", 推流地址
     *          "rtmp_debug", 默认为0, 为1则为debug, 实时显示推流的画面，方便和拉流进行延迟对比
     *
     * @return
     */
        RET_CODE Init(const Properties &properties);

        // pcm数据的数据回调
        void PcmCallback(uint8_t *pcm, int32_t size);
        // 数据回调
        void YuvCallback(uint8_t *yuv, int32_t size);

    private:
        // 音频test模式
        int audio_test_ = 0;
        std::string input_pcm_name_;

        // 麦克风采样属性
        int mic_sample_rate_ = 48000;
        int mic_sample_fmt_ = AV_SAMPLE_FMT_S16;
        int mic_channels_ = 2;

        // 音频编码参数
        int audio_sample_rate_ = AV_SAMPLE_FMT_S16;
        int audio_bitrate_ = 128 * 1024;
        int audio_channels_ = 2;
        int audio_sample_fmt_; // 具体由编码器决定，从编码器读取相应的信息
        int audio_ch_layout_;  // 由audio_channels_决定

        // 视频test模式
        int video_test_ = 0;
        std::string input_yuv_name_;

        // 桌面录制属性
        int desktop_x_ = 0;
        int desktop_y_ = 0;
        int desktop_width_ = 1920;
        int desktop_height_ = 1080;
        int desktop_format_ = AV_PIX_FMT_YUV420P;
        int desktop_fps_ = 25;

        // 视频编码属性
        int video_width_ = 1920;  // 宽
        int video_height_ = 1080; // 高
        int video_fps_;           // 帧率
        int video_gop_;
        int video_bitrate_;
        int video_b_frames_; // b帧数量

        // rtmp推流
        std::string rtmp_url_;
        int rtmp_debug_ = 0;

        RTMPPusher *rtmp_pusher;
        bool need_send_audio_spec_config = true;
        bool need_send_video_config = true;

        // 音频相关
        // 音频编码比较快,直接在pcmCallback进行
        AudioCapturer *audio_capturer_;
        AudioResampler *audio_resampler_;
        AACEncoder *audio_encoder_;
        uint8_t *aac_buf_ = NULL;
        const int AAC_BUF_MAX_LENGTH = 8291 + 64; // 最大为13bit长度(8191), +64 只是防止字节对齐

        // 视频相关
        VideoCapturer *video_capturer = NULL;
        H264Encoder *video_encoder_ = NULL;

        uint8_t *video_nalu_buf = NULL;
        int video_nalu_size_ = 0;
        const int VIDEO_NALU_BUF_MAX_SIZE = 1024 * 1024;

        // 显示推流画面时使用
        VideoOutSDL *video_out_sdl = NULL;

        // dump h264文件
        FILE *h264_fp_ = NULL;
        FILE *aac_fp_ = NULL;
        FILE *pcm_flt_fp_ = NULL;
        FILE *pcm_s16le_fp_ = NULL;
    };
} // namespace LQF
#endif // PUSHWORK_H
