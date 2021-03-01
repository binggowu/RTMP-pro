#ifndef AUDIOCAPTURER_H
#define AUDIOCAPTURER_H

#include "mediabase.h"

namespace LQF
{
    class CommonLooper;

    class AudioCapturer : public CommonLooper
    {
    public:
        AudioCapturer();
        virtual ~AudioCapturer();
        /**
         * @brief Init
         * @param "audio_test": 缺省为0，为1时数据读取本地文件进行播放
         *        "input_pcm_name": 测试模式时读取的文件路径
         *        "sample_rate": 采样率
         *        "channels": 采样通道
         *        "sample_fmt": 采样格式
         * @return
         */
        RET_CODE Init(const Properties &properties);

        virtual void Loop() override;

        void AddCallback(std::function<void(uint8_t *, int32_t)> callback)
        {
            callback_get_pcm_ = callback;
        }

    private:
        // 在Init()中被赋值.
        int audio_test_ = 0;
        std::string input_pcm_name_;
        int sample_rate_;
        uint8_t *pcm_buf_;
        const int PCM_BUF_MAX_SIZE = 32768;
        // int32_t pcm_buf_size_; // 没有使用

        FILE *pcm_fp_ = NULL;

        // PCM file只是用来测试, 写死为s16格式 2通道 采样率48Khz

        int openPcmFile(const std::string &filename);
        int readPcmFile(uint8_t *pcm_buf, int32_t pcm_buf_size);
        int closePcmFile();

        // 用处不大.
        int64_t pcm_start_time_ = 0;    // 起始时间
        double pcm_total_duration_ = 0; // PCM读取累计的时间
        bool is_first_frame_ = false;

        std::function<void(uint8_t *, int32_t)> callback_get_pcm_ = NULL; // PushWork::PcmCallback
    };
}
#endif // AUDIOCAPTURER_H
