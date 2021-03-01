#ifndef VIDEODECODELOOP_H
#define VIDEODECODELOOP_H

#include "mediabase.h"

namespace LQF
{
    class CommonLooper;
    class H264Decoder;

    class VideoDecodeLoop : public CommonLooper
    {
    public:
        VideoDecodeLoop();
        virtual ~VideoDecodeLoop();

        virtual RET_CODE Init(const Properties &properties);

        int GetWdith();
        int GetHeight();

        void AddFrameCallback(std::function<void(void *)> callable_object)
        {
            callable_post_frame_ = callable_object;
        }
        virtual void Loop() override;
        void Post(void *);

    private:
        H264Decoder *h264_decoder_ = NULL;

        std::function<void(void *)> callable_post_frame_ = NULL;

        uint8_t *yuv_buf_;
        int32_t yuv_buf_size_;
        const int YUV_BUF_MAX_SIZE = int(1920 * 1080 * 1.5); // 先写死最大支持, fixme

        // 延迟指标监测
        uint32_t PRINT_MAX_FRAME_DECODE_TIME = 5;
        uint32_t decode_frames_ = 0;
        uint32_t deque_max_size_ = 20;
        PacketQueue *pkt_queue_;

        int pkt_serial; // 包序列
        int packet_cache_delay_;
    };
}

#endif // VideoDecodeLoop_H
