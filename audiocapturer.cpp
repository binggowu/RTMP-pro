#include "commonlooper.h"
#include "dlog.h"
#include "timeutil.h"
#include "avtimebase.h"
#include "audiocapturer.h"

namespace LQF
{
    AudioCapturer::AudioCapturer() {}

    AudioCapturer::~AudioCapturer()
    {
        Stop();
        if (pcm_buf_)
            delete[] pcm_buf_;
    }

    RET_CODE AudioCapturer::Init(const Properties &properties)
    {
        audio_test_ = properties.GetProperty("audio_test", 0);
        input_pcm_name_ = properties.GetProperty("input_pcm_name", "buweishui_48000_2_s16le.pcm");
        pcm_buf_ = new uint8_t[PCM_BUF_MAX_SIZE];
        if (!pcm_buf_)
        {
            return RET_ERR_OUTOFMEMORY;
        }
        return RET_OK;
    }

    // 读取本地pcm文件, 并回调PushWork::PcmCallback
    void AudioCapturer::Loop()
    {
        // LogInfo("into loop");
        int nb_samples = 1024;
        if (openPcmFile(input_pcm_name_) != 0)
        {
            LogError("openPcmFile %s failed", input_pcm_name_.c_str());
            return;
        }
        pcm_total_duration_ = 0;
        pcm_start_time_ = TimesUtil::GetTimeMillisecond();
        // LogInfo("into loop while");
        while (true)
        {
            if (request_exit_)
                break;

            if (readPcmFile(pcm_buf_, nb_samples) == 0)
            {
                if (!is_first_frame_)
                {
                    is_first_frame_ = true;
                    LogInfo("%s:t%u", AVPublishTime::GetInstance()->getAInTag(),
                            AVPublishTime::GetInstance()->getCurrenTime());
                }
                if (callback_get_pcm_)
                {
                    callback_get_pcm_(pcm_buf_, nb_samples * 4);
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        closePcmFile();
    }

    // 打开pcm文件, 给pcm_fp_赋值
    int AudioCapturer::openPcmFile(const std::string &filename)
    {
        pcm_fp_ = fopen(filename.c_str(), "rb");
        if (!pcm_fp_)
        {
            return -1;
        }
        return 0;
    }

    // 读取本地pcm文件
    int AudioCapturer::readPcmFile(uint8_t *pcm_buf, int32_t nb_samples)
    {
        int64_t cur_time = TimesUtil::GetTimeMillisecond();
        int64_t dif = cur_time - pcm_start_time_;
        // printf("%lld, %lld\n", pcm_total_duration_, dif);
        if ((int64_t)pcm_total_duration_ > dif)
            return -1;

        // 该读取数据了
        size_t ret = ::fread(pcm_buf, 1, nb_samples * 4, pcm_fp_);
        if (ret != nb_samples * 4) // 不够读取的, seek到文件开头重试.
        {
            // 从文件头部开始读取
            ret = ::fseek(pcm_fp_, 0, SEEK_SET);
            ret = ::fread(pcm_buf, 1, nb_samples * 4, pcm_fp_);
            if (ret != nb_samples * 4)
            {
                return -1;
            }
        }
        // LogInfo("pcm_total_duration_:%lldms", (int64_t)pcm_total_duration_);
        pcm_total_duration_ += (nb_samples * 1.0 / 48);
        return 0;
    }

    // 关闭pcm_fp_
    int AudioCapturer::closePcmFile()
    {
        if (pcm_fp_)
            fclose(pcm_fp_);
        return 0;
    }
}
