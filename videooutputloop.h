#ifndef VIDEOOUTPUTLOOP_H
#define VIDEOOUTPUTLOOP_H
#include <stdint.h>
#include <functional>
#include "commonlooper.h"
#include "framequeue.h"
using std::function;

namespace LQF
{
    class VideoOutputLoop : public CommonLooper
    {
    public:
        VideoOutputLoop();
        virtual ~VideoOutputLoop()
        {
        }
        RET_CODE Init(const Properties &properties);
        // 负责输出
        virtual void Loop();

        // 用于显示的回调函数
        void AddDisplayCallback(function<void(uint8_t *, uint32_t, int32_t format)> callback)
        {
            callback_display_ = callback;
        }
        // 用于判断是否输出显示的回调函数
        void AddAVSyncCallback(function<int(int64_t, int32_t, int64_t &)> callback)
        {
            callback_avsync_ = callback;
        }
        RET_CODE PushFrame(uint8_t *data, uint32_t size, int64_t pts);

    private:
        std::function<void(uint8_t *, uint32_t, int32_t)> callback_display_ = NULL;
        // pts和帧间隔
        std::function<int(int64_t, int32_t, int64_t &)> callback_avsync_ = NULL;

        FrameQueue *frame_queue_ = NULL;
        int64_t pre_pts_ = 0;

        bool is_show_first_frame_ = false; // 第一帧不做音视频同步
        uint32_t PRINT_MAX_FRAME_OUT_TIME = 5;
        uint32_t out_frames_ = 0; // 统计输出的帧数
    };
}

#endif // VIDEOOUTPUTLOOP_H
