#include "timeutil.h"
#include "avtimebase.h"
#include "audiodecodeloop.h"
#include "videodecodeloop.h"
#include "audiooutsdl.h"
#include "videooutsdl.h"
#include "rtmpplayer.h"
#include "videooutputloop.h"
#include "avsync.h"
#include "audioresample.h"
#include "imagescale.h"
#include "pullwork.h"

namespace LQF
{
    PullWork::PullWork()
    {
    }

    PullWork::~PullWork()
    {
        if (rtmp_player_)
            delete rtmp_player_;
        if (audio_decode_loop_)
            delete audio_decode_loop_;
        if (video_decode_loop_)
            delete video_decode_loop_;
        if (video_output_loop_)
            delete video_output_loop_;
        if (avsync_)
            delete avsync_;
    }

    // 调用 RTMPPlayer::Start()开始工作
    RET_CODE PullWork::Init(const Properties &properties)
    {
        int64_t cur_time = TimesUtil::GetTimeMillisecond();

        rtmp_url_ = properties.GetProperty("rtmp_url", "");
        if ("" == rtmp_url_)
        {
            LogError("rtmp url is null");
            return RET_FAIL;
        }
        video_out_width_ = properties.GetProperty("video_out_width", 480);
        video_out_height_ = properties.GetProperty("video_out_height", 270);
        audio_out_sample_rate_ = properties.GetProperty("audio_out_sample_rate", 48000);
        cache_duration_ = properties.GetProperty("cache_duration", 500);

        // 音视频同步
        avsync_ = new AVSync();
        if (avsync_->Init(AV_SYNC_AUDIO_MASTER) != RET_OK)
        {
            LogError("AVSync Init failed");
            return RET_FAIL;
        }

        // 启动audio解码线程(AudioDecodeLoop::Loop)
        audio_decode_loop_ = new AudioDecodeLoop();
        if (!audio_decode_loop_)
        {
            LogError("new AudioDecodeLoop() failed");
            return RET_FAIL;
        }
        audio_decode_loop_->addFrameCallback(std::bind(&PullWork::pcmFrameCallback, this,
                                                       std::placeholders::_1));
        Properties aud_loop_properties;
        aud_loop_properties.SetProperty("cache_duration", cache_duration_);
        if (audio_decode_loop_->Init(aud_loop_properties) != RET_OK)
        {
            LogError("audio_decode_loop_ Init failed");
            return RET_FAIL;
        }
        if (audio_decode_loop_->Start() != RET_OK)
        {
            LogError("audio_decode_loop_ Start failed");
            return RET_FAIL;
        }

        // audio输出
        audio_out_sdl_ = new AudioOutSDL(avsync_);
        if (!audio_out_sdl_)
        {
            LogError("new AudioOutSDL() failed");
            return RET_FAIL;
        }
        Properties aud_out_properties;
        aud_out_properties.SetProperty("sample_rate", audio_out_sample_rate_);
        aud_out_properties.SetProperty("channels", audio_out_sample_channels_);
        if (audio_out_sdl_->Init(aud_out_properties) != RET_OK)
        {
            LogError("audio_out_sdl Init failed");
            return RET_OK;
        }

        // audio重采样
        audio_resampler_ = new AudioResampler();
        if (!audio_resampler_)
        {
            LogError("new AudioResampler failed");
            return RET_OK;
        }

        // video缩放
        img_scale_ = new ImageScaler();
        if (!img_scale_)
        {
            LogError("new ImageScaler failed");
            return RET_OK;
        }

        // video输出
        video_output_loop_ = new VideoOutputLoop();
        Properties video_out_loop_properties;
        if (video_output_loop_->Init(video_out_loop_properties) != RET_OK)
        {
            LogError("video_output_loop_ Init failed");
            return RET_FAIL;
        }
        video_output_loop_->AddAVSyncCallback(std::bind(&PullWork::avSyncCallback, this,
                                                        std::placeholders::_1,
                                                        std::placeholders::_2,
                                                        std::placeholders::_3));
        video_output_loop_->AddDisplayCallback(std::bind(&PullWork::displayVideo, this,
                                                         std::placeholders::_1,
                                                         std::placeholders::_2,
                                                         std::placeholders::_3));
        if (video_output_loop_->Start() != RET_OK)
        {
            LogError("video_output_loop_ Start   failed");
            return RET_FAIL;
        }

        cur_time = TimesUtil::GetTimeMillisecond();

        // video输出
        video_out_sdl_ = new VideoOutSDL();
        if (!video_out_sdl_)
        {
            LogError("new VideoOutSDL() failed");
            return RET_FAIL;
        }
        Properties vid_out_properties;
        vid_out_properties.SetProperty("video_width", video_out_width_);
        vid_out_properties.SetProperty("video_height", video_out_height_);
        vid_out_properties.SetProperty("win_x", 1000);
        vid_out_properties.SetProperty("win_title", "pull video display");
        if (video_out_sdl_->Init(vid_out_properties) != RET_OK)
        {
            LogError("video_out_sdl Init failed");
            return RET_FAIL;
        }
        // 初始化非常耗时，所以需要提前初始化好 有耗时到1秒
        LogInfo("%s:video_out_init:t:%lld", AVPlayTime::GetInstance()->getKeyTimeTag(),
                TimesUtil::GetTimeMillisecond() - cur_time);

        // 启动video解码线程(VideoDecodeLoop::Loop)
        video_decode_loop_ = new VideoDecodeLoop();
        video_decode_loop_->AddFrameCallback(std::bind(&PullWork::yuvFrameCallback, this,
                                                       std::placeholders::_1));
        if (!video_decode_loop_)
        {
            LogError("new VideoDecodeLoop() failed");
            return RET_FAIL;
        }
        Properties vid_loop_properties;
        if (video_decode_loop_->Init(vid_loop_properties) != RET_OK)
        {
            LogError("audio_decode_loop_ Init failed");
            return RET_FAIL;
        }
        if (video_decode_loop_->Start() != RET_OK)
        {
            LogError("video_decode_loop_ Start   failed");
            return RET_FAIL;
        }

        AVPlayTime::GetInstance()->Rest();
        rtmp_player_ = new RTMPPlayer();
        if (!rtmp_player_)
        {
            LogError("new RTMPPlayer() failed");
            return RET_FAIL;
        }
        rtmp_player_->AddAudioInfoCallback(std::bind(&PullWork::audioInfoCallback, this,
                                                     std::placeholders::_1,
                                                     std::placeholders::_2,
                                                     std::placeholders::_3));
        rtmp_player_->AddVideoInfoCallback(std::bind(&PullWork::videoInfoCallback, this,
                                                     std::placeholders::_1,
                                                     std::placeholders::_2,
                                                     std::placeholders::_3));
        rtmp_player_->AddAudioDataCallback(std::bind(&PullWork::audioDataCallback, this,
                                                     std::placeholders::_1));
        rtmp_player_->AddVideoDataCallback(std::bind(&PullWork::videoDataCallback, this,
                                                     std::placeholders::_1));

        if (!rtmp_player_->Connect(rtmp_url_.c_str()))
        {
            LogError("rtmp_pusher connect() failed");
            return RET_FAIL;
        }
        rtmp_player_->Start();
        return RET_OK;
    }

    void PullWork::audioInfoCallback(int what, MsgBaseObj *data, bool flush)
    {
        int64_t cur_time = TimesUtil::GetTimeMillisecond();
        if (what == RTMP_BODY_AUD_SPEC)
        {
            AudioSpecMsg *spcmsg = (AudioSpecMsg *)data;
            if (!audio_resampler_->IsInit())
            {

                // 用来进行重采样的设置
                AudioResampleParams aud_params;
                aud_params.logtag = "[audio-resample]";
                aud_params.src_sample_fmt = (AVSampleFormat)AV_SAMPLE_FMT_FLTP; // AAC解码器原本输出为fltp, 但封装的时候转成了s16
                aud_params.dst_sample_fmt = (AVSampleFormat)AV_SAMPLE_FMT_S16;
                aud_params.src_sample_rate = spcmsg->sample_rate_;
                aud_params.dst_sample_rate = audio_out_sample_rate_; // 默认使用44.1khz进行测试先
                aud_params.src_channel_layout = av_get_default_channel_layout(spcmsg->channels_);
                aud_params.dst_channel_layout = av_get_default_channel_layout(audio_out_sample_channels_);
                aud_params.logtag = "audio-resample-output";
                audio_resampler_->InitResampler(aud_params);
            }
            delete spcmsg;
        }
        else
        {
            LogError("can't handle what:%d", what);
            delete data;
        }

        int64_t diff = TimesUtil::GetTimeMillisecond() - cur_time;
        if (diff > 5)
            LogDebug("audioCallback t:%ld", diff);
    }

    void PullWork::videoInfoCallback(int what, MsgBaseObj *data, bool flush)
    {
        int64_t cur_time = TimesUtil::GetTimeMillisecond();

        if (what == RTMP_BODY_METADATA)
        {
            return;
        }

        int64_t diff = TimesUtil::GetTimeMillisecond() - cur_time;
        if (diff > 5)
            LogInfo("videoCallback t:%ld", diff);
    }

    // AudioDecoderLoop::Post()
    void PullWork::audioDataCallback(void *pkt)
    {
        audio_decode_loop_->Post(pkt);
    }

    // VideoDecodeLoop::Post()
    void PullWork::videoDataCallback(void *pkt)
    {
        // sps和pps一定要发送过去
        video_decode_loop_->Post(pkt);
    }

    // 重采样 + SDL输出
    // 参数frame: audio解码器解码后得到的一个AVFrame.
    // 调用: AudioDecodeLoop::Loop()
    void PullWork::pcmFrameCallback(void *frame)
    {
        AVFrame *aframe = (AVFrame *)frame;
        auto pts = aframe->pts;
        auto ret = audio_resampler_->SendResampleFrame(aframe);
        if (ret < 0)
        {
            LogError("SendResampleFrame failed ");
            return;
        }
        int getsize = audio_resampler_->GetFifoCurSize();
        auto dstframe = audio_resampler_->ReceiveResampledFrame(getsize);

        // audio输出
        if (audio_out_sdl_)
        {
            int size = av_get_bytes_per_sample((AVSampleFormat)(dstframe->format)) * dstframe->channels * dstframe->nb_samples;
            // 注意: linesize由于数据padding的问题, 不能直接使用其作为数据长度, 否则容易出现各种断音.
            audio_out_sdl_->PushFrame(dstframe->data[0], size, pts);
        }
        else
        {
            LogWarn("audio_out_sdl no open");
        }
    }

    void PullWork::displayVideo(uint8_t *yuv, uint32_t size, int32_t format)
    {
        LogInfo("yuv:%p, size:%d", yuv, size);
        if (video_out_sdl_)
            video_out_sdl_->Output(yuv, size);
        else
            LogWarn("video_out_sdl no open");
    }

    // 尺寸变换 + 
    void PullWork::yuvFrameCallback(void *frame)
    {
        AVFrame *src_frame = (AVFrame *)frame;
        // 尺寸变换
        AVFrame *resizedFrame = av_frame_alloc();
        uint8_t *buffer = (uint8_t *)av_malloc(avpicture_get_size(video_out_format_, video_out_width_, video_out_height_) * sizeof(uint8_t));
        resizedFrame->format = video_out_format_;
        resizedFrame->width = video_out_width_;
        resizedFrame->height = video_out_height_;
        avpicture_fill((AVPicture *)resizedFrame, buffer, video_out_format_, video_out_width_, video_out_height_);
        img_scale_->Scale(src_frame, resizedFrame);

        if (video_output_loop_)
        {
            video_output_loop_->PushFrame(buffer, video_out_width_ * video_out_height_ * 1.5, src_frame->pts);
        }
        else
        {
            LogWarn("video_out_sdl no open");
        }
        av_frame_free(&resizedFrame);
    }

    int PullWork::avSyncCallback(int64_t pts, int32_t duration, int64_t &get_diff)
    {
        return avsync_->GetVideoSyncResult(pts, duration, get_diff);
    }
}
