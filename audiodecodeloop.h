#ifndef AUDIODECODELOOP_H
#define AUDIODECODELOOP_H
#include <deque>
#include <mutex>
#include "mediabase.h"

namespace LQF
{
    class CommonLooper;
    class AACDecoder;
    class PacketQueue;
    class Semaphore;

    class AudioDecodeLoop : public CommonLooper
    {
    public:
        AudioDecodeLoop();
        virtual ~AudioDecodeLoop();

        RET_CODE Init(const Properties &properties);

        void addFrameCallback(std::function<void(void *)> callableObject) { _callable_post_frame_ = callableObject; }

        virtual void Loop() override;
        
        void Post(void *);

    private:
        AACDecoder *aac_decoder_ = NULL;
        std::function<void(void *)> _callable_post_frame_ = NULL; // PullWork::pcmFrameCallback

        PacketQueue *pkt_queue_ = NULL;
        uint32_t deque_max_size_ = 20;

        uint8_t *pcm_buf_;
        int32_t pcm_buf_size_;
        const int PCM_BUF_MAX_SIZE = 32768;

        // 如下3个变量的目的: 让PacketQueue缓存一些AVPacekt再进行解码操作.
        int cache_duration_ = 1000; // 单位秒. 要求PacketQueue中缓存这个多AVPacket后才能进行解码.
        bool cache_enough_ = false; // 配合cache_duration使用. 当PacketQueue中的AVPacekt有cache_duration_时, 该值为true.
        Semaphore semaphore_;       // 配合cache_enough_使用, 当cache_enough_为false时, 阻塞解码线程.

        uint32_t PRINT_MAX_FRAME_DECODE_TIME = 5; // 用于debug
        uint32_t decode_frames_ = 0;              // 解码得到一个AVFrame就+1, 配合PRINT_MAX_FRAME_DECODE_TIME起debug作用.

        int pkt_serial; // 包序列, 没有用到.

        int packet_cache_delay_ = 0; // 用于debug
    };

}

#endif // AUDIODECODELOOP_H
