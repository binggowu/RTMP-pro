#include "avtimebase.h"
#include "h264decoder.h"
#include "commonlooper.h"
#include "mediabase.h"
#include "packetqueue.h"
#include "videodecodeloop.h"

namespace LQF
{
    VideoDecodeLoop::VideoDecodeLoop()
    {
    }

    VideoDecodeLoop::~VideoDecodeLoop()
    {
        pkt_queue_->packet_queue_abort(); // 请求退出队列
        request_exit_ = true;
        Stop();
        if (h264_decoder_)
        {
            delete h264_decoder_;
        }
        if (yuv_buf_)
        {
            delete[] yuv_buf_;
        }
    }

    // 参数properties没有使用.
    RET_CODE VideoDecodeLoop::Init(const Properties &properties)
    {
        h264_decoder_ = new H264Decoder();
        if (!h264_decoder_)
        {
            LogError("new H264Decoder() failed");
            return RET_ERR_OUTOFMEMORY;
        }

        Properties properties2;
        if (h264_decoder_->Init(properties2) != RET_OK)
        {
            LogError("aac_decoder_ Init failed");
            return RET_FAIL;
        }

        yuv_buf_ = new uint8_t[YUV_BUF_MAX_SIZE];
        if (!yuv_buf_)
        {
            LogError("yuv_buf_ new failed");
            return RET_ERR_OUTOFMEMORY;
        }

        pkt_queue_ = new PacketQueue();
        if (!pkt_queue_)
        {
            LogError("PacketQueue new failed");
            return RET_ERR_OUTOFMEMORY;
        }

        if (pkt_queue_->packet_queue_init() != 0)
        {
            return RET_FAIL;
        }
        pkt_queue_->packet_queue_start();
        return RET_OK;
    }

    int VideoDecodeLoop::GetWdith()
    {
        return h264_decoder_->GetWdith();
    }

    int VideoDecodeLoop::GetHeight()
    {
        return h264_decoder_->GetHeight();
    }

    // 从PacketQueue中获得一个AVPacket, 送入解码器.
    // 从解码器获取一个AVFrame,
    void VideoDecodeLoop::Loop()
    {
        AVFrame *frame = av_frame_alloc();
        RET_CODE ret = RET_OK;
        while (true)
        {
            do
            {
                if (request_exit_)
                {
                    break;
                }

                ret = h264_decoder_->ReceiveFrame(frame);
                if (ret == 0)
                {
                    frame->pts = frame->best_effort_timestamp;
                }

                if (ret == RET_OK) // 解到一帧数据AVFrame
                {
                    if (decode_frames_++ < PRINT_MAX_FRAME_DECODE_TIME)
                    {
                        AVPlayTime *play_time = AVPlayTime::GetInstance();
                        LogInfo("%s:c:%u:t:%u", play_time->getAcodecTag(),
                                decode_frames_, play_time->getCurrenTime());
                    }

                    // 和audio的不同地点
                    int width = frame->width;
                    int height = frame->height;
                    uint32_t size = frame->width * frame->height * 1.5;
                    uint8_t *out = yuv_buf_;
                    for (int j = 0; j < height; j++)
                    {
                        memcpy(out + j * width, frame->data[0] + j * frame->linesize[0], width);
                    }
                    out += width * height;
                    for (int j = 0; j < height / 2; j++)
                    {
                        memcpy(out + j * width / 2, frame->data[1] + j * frame->linesize[1], width / 2);
                    }
                    out += width * height / 2 / 2;
                    for (int j = 0; j < height / 2; j++)
                    {
                        memcpy(out + j * width / 2, frame->data[2] + j * frame->linesize[2], width / 2);
                    }

                    // LogDebug("vpts:%lld, dts:%lld", frame->pts, frame->pkt_dts);
                    if (callable_post_frame_)
                    {
                        callable_post_frame_(frame);
                    }
                }
            } while (ret == RET_OK);

            if (request_exit_)
            {
                break;
            }

            AVPacket pkt;
            if (pkt_queue_->packet_queue_get(&pkt, 1, &pkt_serial) < 0)
            {
                LogError("packet_queue_get failed");
                break;
            }
            if (pkt.data != NULL && pkt.size != 0)
            {
                LogDebug("nalu type:%d, size:%d", 0x1f & pkt.data[4], pkt.size);
                if (h264_decoder_->SendPacket(&pkt) != RET_OK)
                {
                    LogError("SendPacket failed, which is an API violation.\n");
                }
                av_packet_unref(&pkt);
            }
            else
            {
                LogWarn("pkt null");
            }
        }
        av_frame_free(&frame);
        LogWarn("Loop leave");
    }

    // 把AVPacekt插入PacketQueue.
    void VideoDecodeLoop::Post(void *pkt)
    {
        auto size = pkt_queue_->get_nb_packets();
        if (size > 15)
        {
            if (packet_cache_delay_++ > 5)
            {
                packet_cache_delay_ = 0; // 只是为了降低打印的频率
                LogInfo("cache %d packet lead to delay\n", size);
            }
        }

        pkt_queue_->packet_queue_put((AVPacket *)pkt);
    }
}
