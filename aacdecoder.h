#ifndef AACDECODER_H
#define AACDECODER_H
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
}
#include "mediabase.h"

namespace LQF
{
    class AACDecoder
    {
    public:
        AACDecoder();
        virtual ~AACDecoder();

        virtual RET_CODE Init(const Properties &properties);
        virtual RET_CODE Decode(const uint8_t *in, int inLen, uint8_t *out, int &outLen);

        virtual RET_CODE SendPacket(const AVPacket *avpkt);
        virtual RET_CODE ReceiveFrame(AVFrame *frame);

        // 获取采样率(sample_rate)
        // virtual uint32_t GetRate() { return ctx->sample_rate; }
        // bool SetConfig(const uint8_t *data, const size_t size);

    private:
        bool inited = false;
        AVCodec *codec = nullptr;
        AVCodecContext *ctx = nullptr;
        AVPacket *packet = nullptr;
        AVFrame *frame = nullptr;
        int numFrameSamples = 1024;
    };
}

#endif // AACDECODER_H
