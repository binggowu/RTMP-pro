#include "audiooutsdl.h"
#include "dlog.h"
#include "timeutil.h"
#include "avsync.h"
#include "avtimebase.h"
#include "framequeue.h"

namespace LQF
{
    static void fill_audio_pcm(void *udata, Uint8 *stream, int len)
    {
        static int64_t s_pre_time = TimesUtil::GetTimeMillisecond();
        AudioOutSDL *audio_out = (AudioOutSDL *)udata;

        int64_t cur_time = TimesUtil::GetTimeMillisecond();
        //    LogInfo("callback fill audio t:%ld", cur_time - s_pre_time);
        int copy_size = 0;
        memset(stream, 0, len);
        if (!audio_out->cache_frame_enough_)
            return;
        while (len > 0)
        {

            if (audio_out->audio_buf_index >= audio_out->audio_buf_size)
            {
                // 帧缓存的数据已经被拷贝完毕，所以要解码才有数据
                // 从队列中获取数据包并进行解码, 非暂停才去读取数据
                if (audio_out->frame_queue_->Size() > 0 && !audio_out->paused)
                {
                    if (audio_out->out_frames_++ < audio_out->PRINT_MAX_FRAME_OUT_TIME)
                    {
                        AVPlayTime *play_time = AVPlayTime::GetInstance();
                        LogInfo("%s:c:%u:t:%u", play_time->getAoutTag(),
                                audio_out->out_frames_, play_time->getCurrenTime());
                    }
                    Frame *frame = audio_out->frame_queue_->Peek();
                    audio_out->audio_clock = frame->pts_;
                    if (frame->Size() > audio_out->audio_buf1_size)
                    {
                        delete[] audio_out->audio_buf1;
                        audio_out->audio_buf1_size = frame->Size();
                        audio_out->audio_buf1 = new uint8_t[audio_out->audio_buf1_size];
                    }
                    memcpy((uint8_t *)audio_out->audio_buf1, frame->Data(), frame->Size());
                    audio_out->audio_buf_index = 0;
                    audio_out->audio_buf = audio_out->audio_buf1;
                    audio_out->audio_buf_size = frame->Size();
                    if (len >= (audio_out->audio_buf_size - audio_out->audio_buf_index))
                    {
                        copy_size = audio_out->audio_buf_size - audio_out->audio_buf_index;
                    }
                    else
                    {
                        copy_size = len;
                    }
                    memset(stream, 0, copy_size);
                    /* 处理音量，实际上是改变PCM数据的幅值 */
                    SDL_MixAudioFormat(stream, (uint8_t *)audio_out->audio_buf + audio_out->audio_buf_index,
                                       AUDIO_S16SYS, copy_size, audio_out->audio_volume);
                    //                memcpy(stream, (uint8_t *)audio_out->audio_buf + audio_out->audio_buf_index, copy_size);
                    audio_out->audio_buf_index += copy_size;
                    len -= copy_size;
                    stream += copy_size;

                    audio_out->frame_queue_->Next(); // 释放帧
                    if (len <= 0)
                        break;
                }
                else
                {
                    LogInfo("no pcm data, t:%u", AVPlayTime::GetInstance()->getCurrenTime());
                    // 如果没有数据则静音
                    memset(stream, 0, len);
                    len = 0;
                    audio_out->cache_frame_enough_ = false;
                    return;
                }
            }
            else
            {
                if (len >= (audio_out->audio_buf_size - audio_out->audio_buf_index))
                {
                    copy_size = audio_out->audio_buf_size - audio_out->audio_buf_index;
                }
                else
                {
                    copy_size = len;
                }
                memset(stream, 0, copy_size);
                /* 处理音量，实际上是改变PCM数据的幅值 */
                SDL_MixAudioFormat(stream, (uint8_t *)audio_out->audio_buf + audio_out->audio_buf_index,
                                   AUDIO_S16SYS, copy_size, audio_out->audio_volume);
                audio_out->audio_buf_index += copy_size;
                len -= copy_size;
                stream += copy_size;
                //             LogInfo("copy_size:%d pcm data, t:%u", copy_size, AVPlayTime::GetInstance()->getCurrenTime());
            }
        }
        audio_out->avsync_->update_audio_pts(audio_out->audio_clock, 1);
    }

    AudioOutSDL::AudioOutSDL(AVSync *avsync)
        : avsync_(avsync)
    {
    }

    AudioOutSDL::~AudioOutSDL()
    {
        LogInfo("~AudioOutSDL()");
        if (audio_buf1)
        {
            SDL_PauseAudio(1);
            // 关闭清理
            // 关闭音频设备
            SDL_CloseAudio();
            SDL_Quit();
            delete[] audio_buf1;
        }
        if (frame_queue_)
        {
            delete frame_queue_;
        }
    }
    RET_CODE AudioOutSDL::Init(const Properties &properties)
    {
        sample_rate_ = properties.GetProperty("sample_rate", 48000);
        sample_fmt_ = properties.GetProperty("sample_fmt", AUDIO_S16SYS);
        channels_ = properties.GetProperty("channels", 2);

        audio_buf1 = new uint8_t[audio_buf1_size]; // 最大帧buffer
        frame_queue_ = new FrameQueue();
        frame_queue_->Init(SAMPLE_QUEUE_SIZE, 0);
        //SDL initialize
        if (SDL_Init(SDL_INIT_AUDIO)) // 支持AUDIO
        {
            LogError("Could not initialize SDL - %s\n", SDL_GetError());
            return RET_FAIL;
        }
        // 音频参数设置SDL_AudioSpec
        spec.freq = sample_rate_;  // 采样频率
        spec.format = sample_fmt_; // 采样点格式
        spec.channels = channels_; // 2通道
        spec.silence = 0;
        spec.samples = 1024;            // 每次读取的采样数量
        spec.callback = fill_audio_pcm; // 回调函数
        spec.userdata = this;

        //打开音频设备
        if (SDL_OpenAudio(&spec, NULL))
        {
            LogError("Failed to open audio device, %s\n", SDL_GetError());
            return RET_FAIL;
        }
        //play audio
        SDL_PauseAudio(0);
        return RET_OK;
    }

    RET_CODE AudioOutSDL::PushFrame(const uint8_t *data,
                                    const uint32_t size, const int64_t pts)
    {
        static FILE *s_aud_out_pcm = fopen("aud_out.pcm", "wb+");
        fwrite(data, size, 1, s_aud_out_pcm);
        fflush(s_aud_out_pcm);

        if (frame_queue_)
        {
            Frame *frame = frame_queue_->PeekWritable();
            if (!frame)
            {
                LogError("PeekWritable failed");
                return RET_FAIL;
            }
            if (frame->Init(data, size, pts) != RET_OK)
            {
                LogError("Frame Init failed");
                return RET_FAIL;
            }

            //        frame->duration_ = 1000;    // 帧间隔
            pre_pts_ = pts;
            frame_queue_->Push(); // 真正插入一帧

            if (frame_queue_->Size() > frame_queue_->MaxSize() / 2)
            {
                cache_frame_enough_ = true;
            }
        }
        else
        {
            LogError("frame_queue_ is null");
            return RET_FAIL;
        }
    }

    void AudioOutSDL::Release()
    {
    }

}
