#ifndef VIDEOCAPTURER_H
#define VIDEOCAPTURER_H

// #include "commonlooper.h"
#include "mediabase.h"

namespace LQF
{
    class CommonLooper;

    class VideoCapturer : public CommonLooper
    {
    public:
        VideoCapturer();
        virtual ~VideoCapturer();

        /**
         * @brief Init
         * @param "x", x起始位置，缺省为0
         *          "y", y起始位置，缺省为0
         *          "width", 宽度，缺省为屏幕宽带
         *          "height", 高度，缺省为屏幕高度
         *          "format", 像素格式，AVPixelFormat对应的值，缺省为AV_PIX_FMT_YUV420P
         *          "fps", 帧数，缺省为25
         * @return
         */
        RET_CODE Init(const Properties &properties);

        virtual void Loop() override;

        void AddCallback(std::function<void(uint8_t *, int32_t)> callable_object)
        {
            callable_object_ = callable_object;
        }

    private:
        // 在Init()中被赋值.
        int video_test_ = 0;
        std::string input_yuv_name_; // yuv文件路径
        int x_;
        int y_;
        int width_ = 0;
        int height_ = 0;
        int pixel_format_ = 0;
        int fps_;
        double frame_duration_ = 40;
        uint8_t *yuv_buf_ = NULL; // 需要释放
        int yuv_buf_size = 0;

        FILE *yuv_fp_ = NULL;

        int openYuvFile(const std::string &filename);
        int readYuvFile(uint8_t *yuv_buf, int32_t yuv_buf_size);
        int closeYuvFile();

        // 用处不大.
        int64_t yuv_start_time_ = 0;    // 起始时间
        double yuv_total_duration_ = 0; // 读取累计的时间
        bool is_first_frame_ = false;

        std::function<void(uint8_t *, int32_t)> callable_object_ = NULL; // PushWork::YuvCallback: 发送SPS/PPS, 或进行编码, 发送nalu数据.
    };
}

#endif // VIDEOCAPTURER_H
