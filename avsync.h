#ifndef AVSYNC_H
#define AVSYNC_H
extern "C"
{
#include "libavutil/time.h"
}
#include "mediabase.h"

/**
和ffplay不一样, 我们统一使用毫秒的方式.
*/

namespace LQF
{
    const int64_t AV_NOSYNC_THRESHOLD = 40;

    enum
    {
        AV_SYNC_AUDIO_MASTER, // 目前仅支持该方式
        AV_SYNC_VIDEO_MASTER,
        AV_SYNC_EXTERNAL_CLOCK, /* synchronize to an external clock */
    };

    enum AV_FRAME_SYNC_RESULT
    {
        AV_FRAME_FREE_RUN, // 有就播放，不做同步
        AV_FRAME_HOLD,     // 等待到时间再播放
        AV_FRAME_DROP,     // 丢掉当前帧
        AV_FRAME_PLAY      // 正常播放
    };

    const int MAX_NAME_SIZE = 20;
    const int64_t INVALID_PTS_VALUE = 0x1fffffffffffffff;

    class Clock
    {
    public:
        Clock(int *queue_serial, const char *name)
        {
            speed_ = 1.0;
            paused_ = 0;
            queue_serial_ = queue_serial;
            if (name)
            {
                strncpy(name_, name, MAX_NAME_SIZE);
                name_[MAX_NAME_SIZE] = '\0';
            }
            else
            {
                name_[0] = '\0';
            }
            set_clock(INVALID_PTS_VALUE, -1);
        }
        int64_t get_clock()
        {
            //        if (*queue_serial_ != serial_)
            //            return INVALID_PTS_VALUE;
            if (paused_)
            {
                return pts_;
            }
            else
            {
                int64_t time = av_gettime_relative() / 1000;
                return (int64_t)(pts_drift_ + time - (time - last_updated_) * (1.0 - speed_));
            }
        }
        void set_clock_at(int64_t pts, int serial, int64_t time)
        {
            pts_ = pts;
            last_updated_ = time;
            pts_drift_ = pts_ - time;
            serial_ = serial;
        }
        void set_clock(int64_t pts, int serial)
        {
            int64_t time = av_gettime_relative() / 1000;
            set_clock_at(pts, serial, time);
        }
        void init_clock(int *queue_serial)
        {
            speed_ = 1.0;
            paused_ = 0;
            queue_serial_ = queue_serial;
            set_clock(INVALID_PTS_VALUE, -1);
        }
        void sync_clock_to_slave(Clock *slave)
        {
            int64_t clock = get_clock();
            int64_t slave_clock = slave->get_clock();
            if (!isInvalid(slave_clock) && (isInvalid(clock) || fabs(clock - slave_clock) > AV_NOSYNC_THRESHOLD))
                slave->set_clock(slave_clock, slave->serial_);
        }
        bool isInvalid(const int64_t pts)
        {
            return pts == INVALID_PTS_VALUE;
        }

    public:
        int64_t pts_;       /* clock base */
        int64_t pts_drift_; /* clock base minus time at which we updated the clock */
        int64_t last_updated_;
        double speed_;
        int serial_; /* clock is based on a packet with this serial */
        int paused_;
        int *queue_serial_; /* pointer to the current packet queue serial, used for obsolete clock*/
        char name_[MAX_NAME_SIZE + 1];
    };

    class AVSync
    {
    public:
        AVSync()
        {
        }

        virtual ~AVSync()
        {
            if (!audclk)
            {
                delete audclk;
            }
            if (!vidclk)
            {
                delete vidclk;
            }
            if (!extclk)
            {
                delete extclk;
            }
        }
        
        bool IsInit()
        {
            return is_init_;
        }
        
        // 设置音视频同步类型
        RET_CODE Init(const int av_sync_type)
        {
            av_sync_type_ = av_sync_type;
            
            // 初始化时钟
            audclk = new Clock(&aud_queue_serial_, "audclk");
            vidclk = new Clock(&vid_queue_serial_, "vidclk");
            extclk = new Clock(&ext_queue_serial_, "extclk");
            if (!audclk || !vidclk || !extclk)
            {
                LogError("new Clock failed");
                return RET_ERR_OUTOFMEMORY;
            }
            
            update_audio_pts(0, 0);
            is_init_ = true;
            return RET_OK;
        }

        int get_master_sync_type()
        {
            if (av_sync_type_ == AV_SYNC_VIDEO_MASTER)
            {
                if (has_video_)
                    return AV_SYNC_VIDEO_MASTER;
                else
                    return AV_SYNC_AUDIO_MASTER;
            }
            else if (av_sync_type_ == AV_SYNC_AUDIO_MASTER)
            {
                if (has_audio_)
                    return AV_SYNC_AUDIO_MASTER;
                else
                    return AV_SYNC_EXTERNAL_CLOCK;
            }
            else
            {
                return AV_SYNC_EXTERNAL_CLOCK;
            }
        }
        
        // 获取当前主时钟值
        int64_t get_master_clock()
        {
            int64_t val;

            switch (get_master_sync_type())
            {
            case AV_SYNC_VIDEO_MASTER:
                val = vidclk->get_clock();
                break;
            case AV_SYNC_AUDIO_MASTER:
                val = audclk->get_clock();
                break;
            default:
                val = extclk->get_clock();
                break;
            }
            return val;
        }

        void update_video_pts(const int64_t pts, const int serial)
        {
            LogDebug("video pts:%lld", pts);
            vidclk->set_clock(pts, serial);
        }
        void update_audio_pts(const int64_t pts, const int serial)
        {
            LogDebug("audio pts:%lld", pts);
            if (abs(pts - audclk->get_clock()) > audio_frame_druation_ / 2)
            {
                audclk->set_clock(pts, serial);
            }
        }

        AV_FRAME_SYNC_RESULT GetVideoSyncResult(const int64_t pts, const int duration, int64_t &get_diff)
        {
            video_frame_druation_ = duration;
            int64_t diff = pts - get_master_clock();
            get_diff = diff;
            LogDebug("vpts:%lld, duration:%d, diff:%lld", pts, duration, diff);
            if (diff > 0 && diff < video_frame_druation_ * 20)
            {
                return AV_FRAME_HOLD;
            }
            else if (diff <= 0 && diff > -video_frame_druation_ / 2)
            {
                return AV_FRAME_PLAY;
            }
            else if (diff <= -video_frame_druation_ / 2)
            {
                return AV_FRAME_DROP;
            }
            else
            {
                // 其他情况自由奔放地播放
                LogWarn("video free run, diff:%lld", diff);
                return AV_FRAME_FREE_RUN;
            }
        }

        int av_sync_type_ = AV_SYNC_AUDIO_MASTER;
        int has_video_ = 1;
        int has_audio_ = 1;
        bool is_init_ = false;
        Clock *audclk = NULL;
        Clock *vidclk = NULL;
        Clock *extclk = NULL;
        int aud_queue_serial_ = 0;
        int vid_queue_serial_ = 0;
        int ext_queue_serial_ = 0;
        int64_t audio_frame_druation_ = 22;
        int64_t video_frame_druation_ = 40;

        int64_t audio_clock_;
        int64_t frame_timer;
        int64_t frame_last_pts;
        int64_t frame_last_delay;

        int64_t video_clock;            /// <pts of last decoded frame / predicted pts of next decoded frame
        int64_t video_current_pts;      /// <current displayed pts (different from video_clock if frame fifos are used)
        int64_t video_current_pts_time; /// <time (av_gettime) at which we updated video_current_pts - used to have running video pts
        static AVSync s_avsync_;
    };
    // AVSync *AVSync::s_avsync_ = NULL;
}

#endif // AVSYNC_H
