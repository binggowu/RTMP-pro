#include "audioresample.h"
#include "dlog.h"

namespace LQF
{
    std::ostream &operator<<(std::ostream &os, const AudioResampleParams &arp)
    {
        const char *srcFmtStr = av_get_sample_fmt_name(arp.src_sample_fmt);
        const char *dstFmtStr = av_get_sample_fmt_name(arp.dst_sample_fmt);
        int srcChannels = av_get_channel_layout_nb_channels(arp.src_channel_layout);
        int dstChannels = av_get_channel_layout_nb_channels(arp.dst_channel_layout);
        os << "AudioResampleParams: \n"
           << "SRC: layout " << arp.src_channel_layout << ", channels " << srcChannels
           << ", samplerate " << arp.src_sample_rate << ", fmt " << srcFmtStr
           << "\nDST: layout " << arp.dst_channel_layout << ", channels " << dstChannels
           << ", samplerate " << arp.dst_sample_rate << ", fmt " << dstFmtStr;
        return os;
    }

    int AudioResampler::InitResampler(const AudioResampleParams &arp)
    {
        int ret = 0;
        resample_params_ = arp;
        logtag_ = resample_params_.logtag;
        /* fifo of output sample format */
        src_channels_ = av_get_channel_layout_nb_channels(resample_params_.src_channel_layout);
        dst_channels_ = av_get_channel_layout_nb_channels(resample_params_.dst_channel_layout);
        audio_fifo_ = av_audio_fifo_alloc(resample_params_.dst_sample_fmt, dst_channels_, 1);
        if (!audio_fifo_)
        {
            LogError("%s av_audio_fifo_alloc failed", logtag_.c_str());
            return -1;
        }

        if (resample_params_.src_sample_fmt == resample_params_.dst_sample_fmt &&
            resample_params_.src_sample_rate == resample_params_.dst_sample_rate &&
            resample_params_.src_channel_layout == resample_params_.dst_channel_layout)
        {
            LogInfo("%s no resample needed, just use audio fifo",
                    logtag_.c_str());
            is_fifo_only = true;
            return 0;
        }

        swr_ctx_ = swr_alloc();

        if (!swr_ctx_)
        {
            LogError("%s swr_alloc failed", logtag_.c_str());
            return -1;
        }
        /* set options */
        ret = 0;
        ret |= av_opt_set_int(swr_ctx_, "in_channel_layout", resample_params_.src_channel_layout, 0);
        ret |= av_opt_set_int(swr_ctx_, "in_sample_rate", resample_params_.src_sample_rate, 0);
        ret |= av_opt_set_sample_fmt(swr_ctx_, "in_sample_fmt", resample_params_.src_sample_fmt, 0);

        ret |= av_opt_set_int(swr_ctx_, "out_channel_layout", resample_params_.dst_channel_layout, 0);
        ret |= av_opt_set_int(swr_ctx_, "out_sample_rate", resample_params_.dst_sample_rate, 0);
        ret |= av_opt_set_sample_fmt(swr_ctx_, "out_sample_fmt", resample_params_.dst_sample_fmt, 0);
        if (ret != 0)
        {
            LogError("av_opt_set_int failed");
            return -1;
        }

        /* initialize the resampling context */
        ret = swr_init(swr_ctx_);
        if (ret < 0)
        {
            LogError("%s  failed to initialize the resampling context.", logtag_.c_str());
            return ret;
        }
        int src_nb_samples = 1024;
        max_dst_nb_samples = dst_nb_samples =
            av_rescale_rnd(src_nb_samples,
                           resample_params_.dst_sample_rate,
                           resample_params_.src_sample_rate, AV_ROUND_UP);

        if (initResampledData() < 0)
            return AVERROR(ENOMEM);
        is_init_ = true;
        LogInfo("%s init done ", logtag_.c_str());
        return 0;
    }

    int AudioResampler::closeResample()
    {
        if (swr_ctx_)
            swr_free(&swr_ctx_);
        if (audio_fifo_)
        {
            av_audio_fifo_free(audio_fifo_);
            audio_fifo_ = nullptr;
        }
        if (resampled_data_)
            av_freep(&resampled_data_[0]);
        av_freep(&resampled_data_);
        return 0;
    }

    int AudioResampler::SendResampleFrame(AVFrame *frame)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        int src_nb_samples = 0;
        uint8_t **src_data = nullptr;
        if (frame)
        {
            src_nb_samples = frame->nb_samples;
            src_data = frame->extended_data;
            if (start_pts_ == AV_NOPTS_VALUE && frame->pts != AV_NOPTS_VALUE)
            {
                start_pts_ = frame->pts;
                cur_pts_ = frame->pts;
            }
        }
        else
        {
            is_flushed = true;
        }

        if (is_fifo_only)
        {
            return src_data ? av_audio_fifo_write(audio_fifo_, (void **)src_data, src_nb_samples) : 0;
        }
        int delay = swr_get_delay(swr_ctx_, resample_params_.src_sample_rate);
        dst_nb_samples = av_rescale_rnd(delay + src_nb_samples,
                                        resample_params_.dst_sample_rate,
                                        resample_params_.src_sample_rate,
                                        AV_ROUND_UP);
        //   const int dst_nb_samples = av_rescale_rnd(src_nb_samples,
        //                                             resample_params_.dst_sample_rate,
        //                                             resample_params_.src_sample_rate,
        //                                             AV_ROUND_UP) + 20;
        //    dst_nb_samples = 941;
        if (dst_nb_samples > max_dst_nb_samples)
        {
            av_freep(&resampled_data_[0]);
            int ret = av_samples_alloc(resampled_data_, &dst_linesize, dst_channels_,
                                       dst_nb_samples, resample_params_.dst_sample_fmt, 1);
            if (ret < 0)
            {
                LogError("av_samples_alloc failed");
                return 0;
            }
            max_dst_nb_samples = dst_nb_samples;
        }
        int nb_samples = swr_convert(swr_ctx_, resampled_data_, dst_nb_samples,
                                     (const uint8_t **)src_data, src_nb_samples);

        int dst_bufsize = av_samples_get_buffer_size(&dst_linesize, dst_channels_,
                                                     nb_samples, resample_params_.dst_sample_fmt, 1);
        // dump
        //
        static FILE *s_swr_fp = fopen("swr.pcm", "wb");
        fwrite(resampled_data_[0], 1, dst_bufsize, s_swr_fp);
        fflush(s_swr_fp);
        //  实际是FIFO的坑
        return av_audio_fifo_write(audio_fifo_, (void **)resampled_data_, nb_samples);
    }

    int AudioResampler::SendResampleFrame(uint8_t *in_pcm, const int in_size)
    {
        if (!in_pcm)
        {
            is_flushed = true;
            return 0;
        }
        auto frame = shared_ptr<AVFrame>(av_frame_alloc(), [](AVFrame *p) {if (p) av_frame_free(&p); });

        frame->format = resample_params_.src_sample_fmt;
        frame->channel_layout = resample_params_.src_channel_layout;
        int ch = av_get_channel_layout_nb_channels(resample_params_.src_channel_layout);
        frame->nb_samples = in_size / av_get_bytes_per_sample(resample_params_.src_sample_fmt) / ch;
        avcodec_fill_audio_frame(frame.get(), ch, resample_params_.src_sample_fmt, in_pcm, in_size, 0);
        int ret = SendResampleFrame(frame.get());
        return ret;
    }

    shared_ptr<AVFrame> AudioResampler::ReceiveResampledFrame(int desired_size)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        desired_size = desired_size == 0 ? av_audio_fifo_size(audio_fifo_) : desired_size;
        if (av_audio_fifo_size(audio_fifo_) < desired_size || desired_size == 0)
            return {};
        /* this call cannot identify the right time of flush, the caller should keep this state */
        return getOneFrame(desired_size);
    }

    int AudioResampler::ReceiveResampledFrame(vector<shared_ptr<AVFrame>> &frames,
                                              int desired_size)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        int ret = 0;
        desired_size = desired_size == 0 ? av_audio_fifo_size(audio_fifo_) : desired_size;
        do
        {
            if (av_audio_fifo_size(audio_fifo_) < desired_size || desired_size == 0)
                break;
            auto frame = getOneFrame(desired_size);
            if (frame)
            {
                frames.push_back(frame);
            }
            else
            {
                ret = AVERROR(ENOMEM);
                break;
            }
        } while (true);

        if (is_flushed)
        {
            ret = AVERROR_EOF;
            frames.push_back(shared_ptr<AVFrame>()); // insert an empty frame
        }
        return ret;
    }

    shared_ptr<AVFrame> AudioResampler::getOneFrame(const int desired_size)
    {
        auto frame = allocOutFrame(desired_size);
        if (frame)
        {
            int ret = av_audio_fifo_read(audio_fifo_, (void **)frame->data, desired_size);
            if (ret <= 0)
            {
                LogError("av_audio_fifo_read failed");
            }
            frame->pts = cur_pts_;
            cur_pts_ += desired_size;
            total_resampled_num_ += desired_size;
        }
        return frame;
    }

    int AudioResampler::initResampledData()
    {
        if (resampled_data_)
            av_freep(&resampled_data_[0]);
        av_freep(&resampled_data_);

        int ret = av_samples_alloc_array_and_samples(&resampled_data_, &dst_linesize, dst_channels_,
                                                     dst_nb_samples, resample_params_.dst_sample_fmt, 0);
        if (ret < 0)
            LogError("%s fail accocate audio resampled data buffer", logtag_.c_str());
        return ret;
    }

    shared_ptr<AVFrame> AudioResampler::allocOutFrame(const int nb_samples)
    {
        int ret;
        auto frame = shared_ptr<AVFrame>(av_frame_alloc(), [](AVFrame *p) {if (p) av_frame_free(&p); });
        if (!frame)
        {
            LogError("%s av_frame_alloc frame failed", logtag_.c_str());
            return {};
        }
        frame->nb_samples = nb_samples;
        frame->channel_layout = resample_params_.dst_channel_layout;
        frame->format = resample_params_.dst_sample_fmt;
        frame->sample_rate = resample_params_.dst_sample_rate;
        ret = av_frame_get_buffer(frame.get(), 0);
        if (ret < 0)
        {
            LogError("%s cannot allocate audio data buffer", logtag_.c_str());
            return {};
        }
        return frame;
    }
}
