#include "aacdecoder.h"
#include "dlog.h"

namespace LQF
{
    AACDecoder::AACDecoder()
    {
    }

    AACDecoder::~AACDecoder()
    {
        if (ctx)
        {
            avcodec_close(ctx);
            av_free(ctx);
        }

        if (packet)
        {
            av_packet_free(&packet);
        }
        if (frame)
        {
            av_frame_free(&frame);
        }
    }

    // 打开解码器(find/alloc/open).
    // 参数properties没有用到.
    RET_CODE AACDecoder::Init(const Properties &properties)
    {
        codec = avcodec_find_decoder(AV_CODEC_ID_AAC);
        if (!codec)
        {
            LogError("No codec found\n");
            return RET_ERR_MISMATCH_CODE;
        }

        ctx = avcodec_alloc_context3(codec);
        // ctx->request_sample_fmt = AV_SAMPLE_FMT_S16;
        ctx->codec_type = AVMEDIA_TYPE_AUDIO;
        ctx->sample_rate = 16000;
        ctx->channels = 2;
        // ctx->bit_rate = bit;
        ctx->channel_layout = AV_CH_LAYOUT_STEREO;
        if (avcodec_open2(ctx, codec, NULL) < 0)
        {
            LogError("could not open codec\n");
            return RET_FAIL;
        }

        packet = av_packet_alloc();
        frame = av_frame_alloc();
        numFrameSamples = 1024;
        return RET_OK;
    }

    // 解码工作, 并转换数据类型为 s16 packet, 写入本地文件.
    RET_CODE AACDecoder::Decode(const uint8_t *in, int inLen, uint8_t *out, int &outLen)
    {
        if (inLen <= 0)
        {
            return RET_FAIL;
        }

        packet->data = (uint8_t *)in;
        packet->size = inLen;
        if (avcodec_send_packet(ctx, packet) < 0)
        {
            LogError("-AACDecoder::Decode() Error decoding AAC packet");
            return RET_FAIL;
        }

        // ???
        av_packet_free_side_data(packet);

        if (avcodec_receive_frame(ctx, frame) < 0)
        {
            outLen = 0;
            return RET_FAIL;
        }

        // float 32bit planner(解码后类型) -> s16 packet
        float *buffer1 = (float *)frame->data[0]; // LLLLLL, 数据类型: float 32bit [-1~1]
        float *buffer2 = (float *)frame->data[1]; // RRRRRR
        auto len = frame->nb_samples;             // audio每通道采样数
        int16_t *sample = (int16_t *)out;
        for (size_t i = 0; i < len; ++i) // lrlrlr
        {
            sample[i * 2] = (int16_t)(buffer1[i] * 0x7fff);
            sample[i * 2 + 1] = (int16_t)(buffer2[i] * 0x7fff);
        }
        outLen = 4096; // ???

        // 解码后的数据(s16 packet)写入本地文件
        //ffplay -ar 48000 -ac 2 -f s16le -i aac_dump.pcm
        static FILE *dump_pcm = NULL;
        if (!dump_pcm)
        {
            dump_pcm = fopen("aac_dump.pcm", "wb");
            if (!dump_pcm)
            {
                LogError("fopen aac_dump.pcm failed");
            }
        }
        if (dump_pcm)
        {
            fwrite(out, 1, outLen, dump_pcm);
            fflush(dump_pcm);
        }
        return RET_OK;
    }

    /**
 * @return RET_OK:  on success, otherwise negative error code:
 *     RET_ERR_EAGAIN:   input is not accepted in the current state - user
 *                         must read output with avcodec_receive_frame() (once
 *                         all output is read, the packet should be resent, and
 *                         the call will not fail with EAGAIN).
 *      RET_ERR_EOF:       the decoder has been flushed, and no new packets can
 *                         be sent to it (also returned if more than 1 flush
 *                         packet is sent)
 *      AVERROR(EINVAL):   codec not opened, it is an encoder, or requires flush
 *      AVERROR(ENOMEM):   failed to add packet to internal queue, or similar
 *      other errors: legitimate decoding errors
 */
    // 解码一个AVPacket
    RET_CODE AACDecoder::SendPacket(const AVPacket *avpkt)
    {
        int ret = avcodec_send_packet(ctx, avpkt);
        if (0 == ret)
        {
            return RET_OK;
        }

        if (AVERROR(EAGAIN) == ret)
        {
            LogWarn("avcodec_send_packet failed, RET_ERR_EAGAIN.\n");
            return RET_ERR_EAGAIN;
        }
        else if (AVERROR_EOF)
        {
            LogWarn("avcodec_send_packet failed, RET_ERR_EOF.\n");
            return RET_ERR_EOF;
        }
        else
        {
            LogError("avcodec_send_packet failed, AVERROR(EINVAL) or AVERROR(ENOMEM) or other...\n");
            return RET_FAIL;
        }
    }

    /**
 * @brief AACDecoder::ReceiveFrame
 * @param frame
 *  *      RET_OK:                 success, a frame was returned
 *      RET_ERR_EAGAIN:   output is not available in this state - user must try
 *                         to send new input
 *      AVERROR_EOF:       the decoder has been fully flushed, and there will be
 *                         no more output frames
 *      AVERROR(EINVAL):   codec not opened, or it is an encoder
 *      other negative values: legitimate decoding errors
 */
    // 从解码器获取一个AVFrame
    RET_CODE AACDecoder::ReceiveFrame(AVFrame *frame)
    {
        int ret = avcodec_receive_frame(ctx, frame);
        if (0 == ret)
        {
            return RET_OK;
        }

        if (AVERROR(EAGAIN) == ret)
        {
            LogWarn("avcodec_receive_frame failed, RET_ERR_EAGAIN.\n");
            return RET_ERR_EAGAIN;
        }
        else if (AVERROR_EOF)
        {
            LogWarn("avcodec_receive_frame failed, RET_ERR_EOF.\n");
            return RET_ERR_EOF;
        }
        else
        {
            LogError("avcodec_receive_frame failed, AVERROR(EINVAL) or AVERROR(ENOMEM) or other...\n");
            return RET_FAIL;
        }
    }

    // 为空, 而且也没有用到, 我先注释掉.
    // bool AACDecoder::SetConfig(const uint8_t *data, const size_t size)
    // {
    //     return true;
    // }
}
