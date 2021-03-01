#ifndef FRAMEQUEUE_H
#define FRAMEQUEUE_H
#include <stdint.h>
#include <string.h>
#include <mutex>
#include <condition_variable>
#include "dlog.h"
#include "mediabase.h"

namespace LQF
{
#define VIDEO_PICTURE_QUEUE_SIZE 3 // 图像帧缓存数量
#define SUBPICTURE_QUEUE_SIZE 16   // 字幕帧缓存数量
#define SAMPLE_QUEUE_SIZE 9        // 采样帧缓存数量
#define FRAME_QUEUE_SIZE SUBPICTURE_QUEUE_SIZE
    using std::conditional;
    using std::mutex;
    class FrameData
    {
    public:
        FrameData(const uint8_t *data, const uint32_t size)
        {
            if (!data)
            {
                LogError("data is null");
                return;
            }

            data_ = new uint8_t[size];
            if (!data_)
            {
                LogError("new uint8_t[%d] failed", size);
                return;
            }
            size_ = size;
            memcpy(data_, data, size_);
        }
        ~FrameData()
        {
            if (data_)
                delete data_;
        }
        // 存放数据
        uint8_t *data_ = NULL;
        uint32_t size_ = 0;
    };

    class Frame
    {
    public:
        Frame()
        {
            frame_ = NULL;
        }
        ~Frame()
        {
            if (frame_)
            {
                delete frame_;
            }
        }
        RET_CODE Init(const uint8_t *data, const uint32_t size, const int64_t pts)
        {
            if (frame_)
            {
                delete frame_;
                frame_ = NULL;
            }
            frame_ = new FrameData(data, size);
            pts_ = pts;
            return RET_OK;
        }
        uint8_t *Data()
        {
            if (frame_)
            {
                return frame_->data_;
            }
            else
            {
                return NULL;
            }
        }
        uint32_t Size()
        {
            if (frame_)
            {
                return frame_->size_;
            }
            else
            {
                return 0;
            }
        }

        FrameData *frame_ = NULL;

        int64_t pts_ = 0; // 毫秒

        int64_t duration_ = 40; // 持续时间, 默认40毫秒

        int64_t pos_ = 0; // 该帧在输入文件中的字节位置
        int width_ = 0;   // 图像宽度
        int height_ = 0;  // 图像高读
        int format_ = 0;  // 对于图像为(enum AVPixelFormat)，对于声音则为(enum AVSampleFormat)
        int serial_ = 0;  //帧序列，在seek的操作时serial会变化
        int uploaded = 0; // 用来记录该帧是否已经显示过？
        int flip_v = 0;   // =1则旋转180， = 0则正常播放
        //    AVRational	sar;            // 图像的宽高比，如果未知或未指定则为0/1
    };

    class FrameQueue
    {
    public:
        FrameQueue() {}

        RET_CODE Init(int max_size, int keep_last)
        {
            max_size_ = FRAME_QUEUE_SIZE;
            if (max_size_ > max_size)
                max_size_ = max_size;

            keep_last_ = keep_last;
            rindex_ = 0;
            rindex_shown_ = 0;
            size_ = 0;
            windex_ = 0;
            return RET_OK;
        }
        void Destory()
        {
            for (int i = 0; i < max_size_; i++)
            {
                if (queue_[i].frame_)
                {
                    delete queue_[i].frame_;
                    queue_[i].frame_ = NULL;
                }
            }
        }
        void Signal()
        {
            std::unique_lock<std::mutex> lck(mutex_);
            cond_.notify_one();
        }

        // 从队列取出当前需要显示的一帧
        Frame *Peek()
        {
            return &queue_[(rindex_ + rindex_shown_) % max_size_];
        }
        // 从队列取出当前需要显示的下一帧
        Frame *PeekNext()
        {
            return &queue_[(rindex_ + rindex_shown_ + 1) % max_size_];
        }
        // 从队列取出最近被播放的一帧
        Frame *PeekLast()
        {
            return &queue_[rindex_];
        }
        // 检测队列是否可写空间
        Frame *PeekWritable()
        {
            /* wait until we have space to put a new frame */
            std::unique_lock<std::mutex> lck(mutex_);
            while (size_ >= max_size_)
            { /* 检查是否需要退出 */
                cond_.wait(lck);
            }

            //        if (f->pktq->abort_request)		 /* 检查是不是要退出 */
            //            return NULL;

            return &queue_[windex_];
        }
        // 检测队列是否有数据可读
        Frame *PeekReadable()
        {
            /* wait until we have a readable a new frame */
            std::unique_lock<std::mutex> lck(mutex_);
            while (size_ - rindex_shown_ <= 0)
            {
                cond_.wait(lck);
            }

            //        if (f->pktq->abort_request)
            //            return NULL;

            return &queue_[(rindex_ + rindex_shown_) % max_size_];
        }
        // 队列真正增加一帧，frame_queue_peek_writable只是获取一个可写的位置，但计数器没有修改
        void Push()
        {
            if (++windex_ == max_size_) /* 循环写入 */
                windex_ = 0;
            std::unique_lock<std::mutex> lck(mutex_);
            size_++;
            cond_.notify_one();
        }
        // 真正减少一帧数据，从帧队列中取出帧之后的参数操作，当rindex_show为0的时候使其变为1，否则出队列一帧
        int Next()
        {
            if (keep_last_ && !rindex_shown_)
            {
                rindex_shown_ = 1;
                return 0;
            }
            delete queue_[rindex_].frame_;
            queue_[rindex_].frame_ = NULL;

            if (++rindex_ == max_size_)
                rindex_ = 0;
            std::unique_lock<std::mutex> lck(mutex_);
            size_--;
            cond_.notify_one();
            return 1;
        }
        // 剩余帧数
        int Size()
        {
            return size_ - rindex_shown_; // 注意这里为什么要减去rindex_shown_
        }
        int MaxSize()
        {
            return max_size_; // 注意这里为什么要减去rindex_shown_
        }

    private:
        Frame queue_[FRAME_QUEUE_SIZE];   // FRAME_QUEUE_SIZE  最大size, 数字太大时会占用大量的内存，需要注意该值的设置
        int rindex_ = 0;                  // 表示循环队列的结尾处
        int windex_ = 0;                  // 表示循环队列的开始处
        int size_ = 0;                    // 当前队列的有效帧数
        int max_size_ = FRAME_QUEUE_SIZE; // 当前队列最大的帧数容量
        int keep_last_ = 0;               // = 1说明要在队列里面保持最后一帧的数据不释放，只在销毁队列的时候才将其真正释放
        int rindex_shown_ = 0;            // 初始化为0，配合keep_last=1使用
        std::mutex mutex_;                // 互斥量
        std::condition_variable cond_;    // 条件变量
    };
}
#endif // FRAMEQUEUE_H
