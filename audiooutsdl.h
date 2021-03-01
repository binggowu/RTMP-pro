#ifndef AUDIOOUTSDL_H
#define AUDIOOUTSDL_H

#include <mutex>
#include <SDL2/SDL.h>
// #include "framequeue.h"
// #include "mediabase.h"

namespace LQF
{
    class AVSync;
    class FrameQueue;

    class AudioOutSDL
    {
    public:
        AudioOutSDL(AVSync *avsync);
        virtual ~AudioOutSDL();

        /**
     * @brief Init
     * @param   "sample_fmt", 采样格式 AVSampleFormat对应的值，缺省为AV_SAMPLE_FMT_S16
     *          "sample_rate", 采样率，缺省为480000
     *          "channels",  采样通道，缺省为2通道
     * @return
     */
        virtual RET_CODE Init(const Properties &properties);
        RET_CODE PushFrame(const uint8_t *data, const uint32_t size, const int64_t pts);
        void Release();

    public:
        std::mutex lock_; //
        FrameQueue *frame_queue_ = NULL;
        int64_t audio_clock = 0;
        int audio_volume = 100;
        AVSync *avsync_ = NULL;

        uint8_t *audio_buf = NULL;  // 指向解码后的数据，它只是一个指针，实际存储解码后的数据在audio_buf1
        uint8_t *audio_buf1 = NULL; //
        uint32_t audio_buf1_size = 4096 * 4;
        uint32_t audio_buf_size = 0;  // audio_buf指向数据帧的数据长度，以字节为单位
        uint32_t audio_buf_index = 0; // audio_buf_index当前读取的位置，不能超过audio_buf_size
        int paused = 0;
        uint32_t PRINT_MAX_FRAME_OUT_TIME = 5;
        uint32_t out_frames_ = 0; // 统计输出的帧数
        bool cache_frame_enough_ = false;

    private:
        SDL_AudioSpec spec;
        int sample_rate_ = 48000;
        int sample_fmt_ = AUDIO_S16SYS;
        int channels_ = 2;

        int64_t pre_pts_ = 0;
    };
}
#endif // AUDIOOUTSDL_H
