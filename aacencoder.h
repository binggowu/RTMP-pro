#ifndef AACENCODER_H
#define AACENCODER_H
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
}
#include "mediabase.h"
#include "codecs.h"
#include "audio.h"

class AACEncoder
{
public:
    AACEncoder();
    virtual ~AACEncoder();

    /**
     * @brief Init
     * @param "sample_rate", 采样率，默认48000
     *        "channels", 通道数，默认2
     *        "bitrate", 比特率, 默认128*1024
     *        "channel_layout", 通道布局,  默认根据channels获取缺省的布局
     * @return
     */
    RET_CODE Init(const Properties &properties);

    virtual int Encode(AVFrame *frame, uint8_t *out, int out_len);
    virtual RET_CODE EncodeInput(const uint8_t *in, const uint32_t size);
    virtual RET_CODE EncodeInput(const AVFrame *frame);
    virtual RET_CODE EncodeOutput(AVPacket *pkt);
    virtual RET_CODE EncodeOutput(uint8_t *out, uint32_t &size);

    virtual int get_sample_rate() { return ctx_->sample_rate; }
    virtual int get_sample_format() { return ctx_->sample_fmt; }
    virtual int get_channels() { return ctx_->channels; }
    virtual uint64_t get_channel_layout() { return ctx_->channel_layout; }
    virtual int get_profile() { return ctx_->profile; }
    virtual int64_t get_bit_rate() { return ctx_->bit_rate; }
    // virtual uint32_t GetRate() { return ctx_->sample_rate ? ctx_->sample_rate : 8000; }
    // 每通道需要的sample数量
    virtual uint32_t GetFrameSampleSize() { return ctx_->frame_size; }
    // 一帧数据总共需要的字节数 每个sample占用字节*channels*GetFrameSampleSize
    virtual uint32_t GetFrameByteSize() { return frame_byte_size_; }

    void GetAdtsHeader(uint8_t *adts_header, int aac_length)
    {
        uint8_t freqIdx = 0; //0: 96000 Hz  3: 48000 Hz 4: 44100 Hz
        switch (ctx_->sample_rate)
        {
        case 96000:
            freqIdx = 0;
            break;
        case 88200:
            freqIdx = 1;
            break;
        case 64000:
            freqIdx = 2;
            break;
        case 48000:
            freqIdx = 3;
            break;
        case 44100:
            freqIdx = 4;
            break;
        case 32000:
            freqIdx = 5;
            break;
        case 24000:
            freqIdx = 6;
            break;
        case 22050:
            freqIdx = 7;
            break;
        case 16000:
            freqIdx = 8;
            break;
        case 12000:
            freqIdx = 9;
            break;
        case 11025:
            freqIdx = 10;
            break;
        case 8000:
            freqIdx = 11;
            break;
        case 7350:
            freqIdx = 12;
            break;
        default:
            LogError("can't support sample_rate:%d");
            freqIdx = 4;
            break;
        }
        uint8_t ch_cfg = ctx_->channels;
        uint32_t frame_length = aac_length + 7;
        adts_header[0] = 0xFF;
        adts_header[1] = 0xF1;
        adts_header[2] = ((ctx_->profile) << 6) + (freqIdx << 2) + (ch_cfg >> 2);
        adts_header[3] = (((ch_cfg & 3) << 6) + (frame_length >> 11));
        adts_header[4] = ((frame_length & 0x7FF) >> 3);
        adts_header[5] = (((frame_length & 7) << 5) + 0x1F);
        adts_header[6] = 0xFC;
    }

private:
    // 需要配置的参数
    int sample_rate_ = 48000;
    int channels_ = 2;
    int bitrate_ = 128 * 1024;
    int channel_layout_; //  默认 AV_CH_LAYOUT_STEREO

    AVCodec *codec_;
    AVCodecContext *ctx_;
    AVFrame *frame_;

    // int samplesSize_;
    // int samplesNum_;

    AudioCodec::Type type_;
    int frame_byte_size_; // 一帧的输入byte size
};

#endif // AACENCODER_H
