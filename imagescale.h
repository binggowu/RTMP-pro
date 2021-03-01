#ifndef IMAGESCALE_H
#define IMAGESCALE_H
extern "C"
{
#include "libavutil/imgutils.h"
#include "libavutil/opt.h"
#include "libavcodec/avcodec.h"
#include "libavutil/mathematics.h"
#include "libavutil/samplefmt.h"
#include "libswscale/swscale.h"
}

#include "mediabase.h"

namespace LQF
{
    // Scale算法
    enum SwsAlogrithm
    {
        SWS_SA_FAST_BILINEAR = 0x1,
        SWS_SA_BILINEAR = 0x2,
        SWS_SA_BICUBIC = 0x4,
        SWS_SA_X = 0x8,
        SWS_SA_POINT = 0x10,
        SWS_SA_AREA = 0x20,
        SWS_SA_BICUBLIN = 0x40,
        SWS_SA_GAUSS = 0x80,
        SWS_SA_SINC = 0x100,
        SWS_SA_LANCZOS = 0x200,
        SWS_SA_SPLINE = 0x400,
    };

    class ImageScaler
    {
    public:
        ImageScaler()
        {
            sws_ctx_ = NULL;
            src_pix_fmt_ = AV_PIX_FMT_NONE;
            dst_pix_fmt_ = AV_PIX_FMT_NONE;
            en_alogrithm_ = SWS_SA_FAST_BILINEAR;

            src_width_ = src_height_ = 0;
            m_nSrcPicth = 0;
            dst_width_ = dst_height_ = 0;
            m_nDstPicth = 0;
            for (int i = 0; i < 4; i++)
            {
                m_nSrcSlice[i] = -1;
                m_nSrcStride[i] = 0;
                m_nDstSlice[i] = -1;
                m_nDstStride[i] = 0;
            }
        }

        ~ImageScaler(void)
        {
            DeInit();
        }

        RET_CODE Init(uint32_t src_width, uint32_t src_height, int src_pix_fmt,
                      uint32_t dst_width, uint32_t dst_height, int dst_pix_fmt,
                      int en_alogrithm)
        {
            src_width_ = src_width;
            src_height_ = src_height;
            src_pix_fmt_ = (AVPixelFormat)src_pix_fmt;
            dst_width_ = dst_width;
            dst_height_ = dst_height;
            dst_pix_fmt_ = (AVPixelFormat)dst_pix_fmt;
            en_alogrithm_ = en_alogrithm;
            
            sws_ctx_ = sws_getContext(
                src_width_,
                src_height_,
                (AVPixelFormat)src_pix_fmt_,
                dst_width_,
                dst_height_,
                (AVPixelFormat)dst_pix_fmt_,
                (int)en_alogrithm_,
                NULL,
                NULL,
                NULL);
            if (!sws_ctx_)
            {
                LogError("Impossible to create scale context for the conversion "
                         "fmt:%s s:%dx%d -> fmt:%s s:%dx%d\n",
                         av_get_pix_fmt_name(src_pix_fmt_), src_width_, src_height_,
                         av_get_pix_fmt_name(dst_pix_fmt_), dst_width_, dst_height_);
                return RET_FAIL;
            }
            return RET_OK;
        }

        void DeInit()
        {
            if (sws_ctx_)
            {
                sws_freeContext(sws_ctx_);
                sws_ctx_ = NULL;
            }
        }

        RET_CODE Scale(const AVFrame *src_frame, AVFrame *dst_frame)
        {
            if (src_frame->width != src_width_ || src_frame->height != src_height_ || src_frame->format != src_pix_fmt_ ||
                dst_frame->width != dst_width_ || dst_frame->height != dst_height_ || dst_frame->format != dst_pix_fmt_ || !sws_ctx_)
            {
                // 重新初始化
                DeInit();
                RET_CODE ret = Init(src_frame->width, src_frame->height, src_frame->format,
                                    dst_frame->width, dst_frame->height, dst_frame->format,
                                    en_alogrithm_);
                if (ret != RET_OK)
                {
                    LogError("Init failed");
                    return ret;
                }
            }

            int dst_slice_h = sws_scale(sws_ctx_, (const uint8_t **)src_frame->data, src_frame->linesize, 0, src_frame->height,
                                        dst_frame->data, dst_frame->linesize);
            if (dst_slice_h > 0)
                return RET_OK;
            else
                return RET_FAIL;
        }

    private:
        SwsContext *sws_ctx_;
        AVPixelFormat src_pix_fmt_; // 源像素格式
        AVPixelFormat dst_pix_fmt_; // 目标像素格式

        int en_alogrithm_ = SWS_SA_FAST_BILINEAR; // Resize算法

        int src_width_, src_height_; // 源图像宽高
        int m_nSrcPicth;             // 源图像第一行数据的长度
        int m_nSrcSlice[4];          // 源图像各分量数据起始地址偏移
        int m_nSrcStride[4];         // 源图像各分量一行数据的长度

        int dst_width_, dst_height_; // 目标图像宽高
        int m_nDstPicth;             // 目标图像第一行数据的长度
        int m_nDstSlice[4];          // 目标图像各分量数据起始地址偏移
        int m_nDstStride[4];         // 目标图像各分量一行数据的长度

        struct SwsContext *img_convert_ctx;
        int width, height, xleft, ytop;
    };

}

#endif // IMAGESCALE_H
