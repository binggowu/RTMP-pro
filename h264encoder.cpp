#include "h264encoder.h"
#include "dlog.h"
namespace LQF
{
    H264Encoder::H264Encoder() {}

    H264Encoder::~H264Encoder()
    {
        if (ctx_)
            avcodec_close(ctx_);
        if (frame_)
            av_free(frame_);
        if (picture_buf_)
            av_free(picture_buf_);
        av_free_packet(&packet_);
    }

    /**
     * @brief 编码codec 的time_base实际上表示视频的帧率，
     * qmin,qmax决定了编码的质量，qmin，qmax越大编码的质量越差。
     * zerolatency参数的作用是提搞编码的实时性。
     * 更详细的设置参考:
     * https://trac.ffmpeg.org/wiki/Encode/H.264
     * ffmpeg libx264 h264_nvenc 编码参数解析 https://blog.csdn.net/leiflyy/article/details/87935084
     */
    int H264Encoder::Init(const Properties &properties)
    {
        width_ = properties.GetProperty("width", 0);
        if (width_ == 0 || width_ % 2 != 0)
        {
            LogError("width:%d", width_);
            return RET_ERR_NOT_SUPPORT;
        }
        height_ = properties.GetProperty("height", 0);
        if (height_ == 0 || height_ % 2 != 0)
        {
            LogError("height:%d", height_);
            return RET_ERR_NOT_SUPPORT;
        }
        fps_ = properties.GetProperty("fps", 25);
        b_frames_ = properties.GetProperty("b_frames", 0);
        bitrate_ = properties.GetProperty("bitrate", 500 * 1024);
        gop_ = properties.GetProperty("gop", fps_);

        // 寻找编码器
        codec_ = avcodec_find_encoder(AV_CODEC_ID_H264);
        if (!codec_)
        {
            printf("Can not find encoder! \n");
        }

        count = 0;
        framecnt = 0;

        ctx_ = avcodec_alloc_context3(codec_);

        //Param that must set

        //最大和最小量化系数，取值范围为0~51。
        ctx_->qmin = 10;
        ctx_->qmax = 31;
        //编码后的视频帧大小，以像素为单位。
        ctx_->width = width_;
        ctx_->height = height_;
        //编码后的码率：值越大越清晰，值越小越流畅。
        ctx_->bit_rate = bitrate_;
        //每20帧插入一个I帧
        ctx_->gop_size = gop_;
        //帧率的基本单位，time_base.num为时间线分子，time_base.den为时间线分母，帧率=分子/分母。
        ctx_->time_base.num = 1;
        ctx_->time_base.den = fps_;

        ctx_->framerate.num = fps_;
        ctx_->framerate.den = 1;

        //图像色彩空间的格式，采用什么样的色彩空间来表明一个像素点。
        ctx_->pix_fmt = AV_PIX_FMT_YUV420P;

        //编码器编码的数据类型
        ctx_->codec_type = AVMEDIA_TYPE_VIDEO;

        //Optional Param
        //两个非B帧之间允许出现多少个B帧数，设置0表示不使用B帧，没有编码延时。B帧越多，压缩率越高。
        ctx_->max_b_frames = b_frames_;

        if (ctx_->codec_id == AV_CODEC_ID_H264)
        {
            av_dict_set(&param, "preset", "ultrafast", 0);
            av_dict_set(&param, "tune", "zerolatency", 0);
        }
        if (ctx_->codec_id == AV_CODEC_ID_H265)
        {
            av_dict_set(&param, "preset", "ultrafast", 0);
            av_dict_set(&param, "tune", "zero-latency", 0);
        }

        ctx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER; //extradata拷贝 sps pps
        //初始化视音频编码器的AVCodecContext
        if (avcodec_open2(ctx_, codec_, &param) < 0)
        {
            printf("Failed to open encoder! \n");
        }
        // 读取sps pps 信息
        if (ctx_->extradata)
        {
            LogWarn("extradata_size:%d", ctx_->extradata_size);
            // 第一个为sps 7
            // 第二个为pps 8

            uint8_t *sps = ctx_->extradata + 4; // 直接跳到数据
            int sps_len = 0;
            uint8_t *pps = NULL;
            int pps_len = 0;
            uint8_t *data = ctx_->extradata + 4;
            for (int i = 0; i < ctx_->extradata_size - 4; ++i)
            {
                if (0 == data[i] && 0 == data[i + 1] && 0 == data[i + 2] && 1 == data[i + 3])
                {
                    pps = &data[i + 4];
                    break;
                }
            }
            sps_len = int(pps - sps) - 4; // 4是00 00 00 01占用的字节
            pps_len = ctx_->extradata_size - 4 * 2 - sps_len;
            sps_.append(sps, sps + sps_len);
            pps_.append(pps, pps + pps_len);
        }

        // Init frame
        frame_ = av_frame_alloc();
        int pictureSize = avpicture_get_size(ctx_->pix_fmt, ctx_->width, ctx_->height);
        picture_buf_ = (uint8_t *)av_malloc(pictureSize);
        avpicture_fill((AVPicture *)frame_, picture_buf_, ctx_->pix_fmt, ctx_->width, ctx_->height);

        frame_->width = ctx_->width;
        frame_->height = ctx_->height;
        frame_->format = ctx_->pix_fmt;

        // Init packet
        av_new_packet(&packet_, pictureSize);
        data_size_ = ctx_->width * ctx_->height;

        return 0;
    }

    // 调用: PushWork::YuvCallback()
    int H264Encoder::Encode(uint8_t *in, int in_samples, uint8_t *out, int &out_size)
    {
        frame_->data[0] = in;                                                           // Y
        frame_->data[1] = in + data_size_;                                              // U
        frame_->data[2] = in + data_size_ * 5 / 4;                                      // V
        frame_->pts = (count++) * (ctx_->time_base.den) / ((ctx_->time_base.num) * 25); //时间戳

        av_init_packet(&packet_);
        //Encode
        int got_picture = 0;
        int ret = avcodec_encode_video2(ctx_, &packet_, frame_, &got_picture);

        if (ret < 0)
        {
            printf("Failed to encode! \n");
            return -1;
        }

        if (got_picture == 1)
        {
            //        printf("Succeed to encode frame: %5d\tsize:%5d\n", framecnt, packet.size);
            framecnt++;
            // 跳过00 00 00 01 startcode nalu
            memcpy(out, packet_.data + 4, packet_.size - 4);
            out_size = packet_.size - 4;
            av_packet_unref(&packet_); //释放内存 不释放则内存泄漏
            return 0;
        }

        return -1;
    }

    int H264Encoder::Encode(AVFrame *frame, uint8_t *out, int &out_size)
    {
        return 0;
    }

    int H264Encoder::get_sps(uint8_t *sps, int &sps_len)
    {
        if (!sps || sps_len < sps_.size())
        {
            LogError("sps or sps_len failed");
            return -1;
        }
        sps_len = sps_.size();
        memcpy(sps, sps_.c_str(), sps_len);
        return 0;
    }

    int H264Encoder::get_pps(uint8_t *pps, int &pps_len)
    {
        if (!pps || pps_len < pps_.size())
        {
            LogError("pps or pps_len failed");
            return -1;
        }
        pps_len = pps_.size();
        memcpy(pps, pps_.c_str(), pps_len);
        return 0;
    }
} // namespace LQF
