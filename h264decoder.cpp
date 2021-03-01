#include "h264decoder.h"

/* 3 zero uint8_ts syncword */
static const uint8_t sync_uint8_ts[] = {0, 0, 0, 1};

H264Decoder::H264Decoder()
{
    codec_ = NULL;
    ctx_ = NULL;
}

H264Decoder::~H264Decoder()
{
    if (ctx_)
    {
        avcodec_free_context(&ctx_);
    }
    if (picture_)
        av_frame_free(&picture_);
}

// 打开解码器(find/alloc/open).
// 参数properties没有用到.
RET_CODE H264Decoder::Init(const Properties &properties)
{
    codec_ = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec_)
    {
        LogError("No decoder found\n");
        return RET_ERR_MISMATCH_CODE;
    }

    ctx_ = avcodec_alloc_context3(codec_);
    if (!ctx_)
    {
        LogError("avcodec_alloc_context3 failed\n");
        return RET_ERR_OUTOFMEMORY;
    }

    // ???
    if (codec_->capabilities & AV_CODEC_CAP_TRUNCATED)
    {
        /* we do not send complete frames */
        ctx_->flags |= AV_CODEC_FLAG_TRUNCATED;
    }
    if (avcodec_open2(ctx_, codec_, NULL) != 0)
    {
        LogError("avcodec_open2 %s failed\n", codec_->name);
        // avcodec_close(ctx_);
        // free(ctx_);
        // ctx_ = NULL;
        return RET_FAIL;
    }

    picture_ = av_frame_alloc();
    if (!picture_)
    {
        LogError("av_frame_alloc failed\n");
        return RET_ERR_OUTOFMEMORY;
    }

    return RET_OK;
}

// 解码工作, 并转换数据类型为 packet, 写入本地文件.
RET_CODE H264Decoder::Decode(uint8_t *in, int32_t in_len, uint8_t *out, int32_t &out_len)
{
    int got_picture = 0;

    AVPacket pkt;
    av_init_packet(&pkt);
    pkt.data = in;
    pkt.size = in_len;

    static FILE *dump_h264 = NULL;
    if (!dump_h264)
    {
        dump_h264 = fopen("dump_h2642.h264", "wb");
        if (!dump_h264)
        {
            LogError("fopen dump_h2642.h264 failed");
        }
    }
    if (dump_h264)
    {
        // ffplay -ar 48000 -ac 2 -f s16le -i aac_dump.pcm
        fwrite(in, 1, in_len, dump_h264); // Y
        fflush(dump_h264);
    }

    int readed = avcodec_decode_video2(ctx_, picture_, &got_picture, &pkt);

    // Si hay picture_
    if (got_picture && readed > 0)
    {
        if (ctx_->width == 0 || ctx_->height == 0)
        {
            LogError("-Wrong dimmensions [%d,%d]\n", ctx_->width, ctx_->height);
            return RET_FAIL;
        }
        int width = picture_->width;
        int height = picture_->height;
        out_len = width * height * 1.5;
        // 转换: planner -> packet
        for (int j = 0; j < height; j++)
        {
            memcpy(out + j * width, picture_->data[0] + j * picture_->linesize[0], width);
        }
        out += width * height;
        for (int j = 0; j < height / 2; j++)
        {
            memcpy(out + j * width / 2, picture_->data[1] + j * picture_->linesize[1], width / 2);
        }
        out += width * height / 2 / 2;
        for (int j = 0; j < height / 2; j++)
        {
            memcpy(out + j * width / 2, picture_->data[2] + j * picture_->linesize[2], width / 2);
        }

        static FILE *dump_yuv = NULL;
        if (!dump_yuv)
        {
            dump_yuv = fopen("h264_dump_320x240.yuv", "wb");
            if (!dump_yuv)
            {
                LogError("fopen h264_dump.yuv failed");
            }
        }
        if (dump_yuv)
        {
            // ffplay -ar 48000 -ac 2 -f s16le -i aac_dump.pcm
            // AVFrame存储YUV420P对齐分析
            // https://blog.csdn.net/dancing_night/article/details/80830920?depth_1-utm_source=distribute.pc_relevant.none-task&utm_source=distribute.pc_relevant.none-task
            for (int j = 0; j < height; j++)
                fwrite(picture_->data[0] + j * picture_->linesize[0], 1, width, dump_yuv);
            for (int j = 0; j < height / 2; j++)
                fwrite(picture_->data[1] + j * picture_->linesize[1], 1, width / 2, dump_yuv);
            for (int j = 0; j < height / 2; j++)
                fwrite(picture_->data[2] + j * picture_->linesize[2], 1, width / 2, dump_yuv);

            fflush(dump_yuv);
        }
        return RET_OK;
    }
    out_len = 0;
    return RET_ERR_EAGAIN;
}

/**
 * @brief H264Decoder::SendPacket
 * @param avpkt
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
RET_CODE H264Decoder::SendPacket(const AVPacket *avpkt)
{
    int ret = avcodec_send_packet(ctx_, avpkt);
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
 * @brief H264Decoder::ReceiveFrame
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
RET_CODE H264Decoder::ReceiveFrame(AVFrame *frame)
{
    int ret = avcodec_receive_frame(ctx_, frame);
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
