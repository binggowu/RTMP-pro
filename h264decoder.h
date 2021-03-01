#ifndef H264DECODER_H
#define H264DECODER_H
extern "C"
{
#include <libavcodec/avcodec.h>
}
#include "mediabase.h"

class H264Decoder
{
public:
    H264Decoder();
    virtual ~H264Decoder();
    
    virtual RET_CODE Init(const Properties &properties);
    virtual RET_CODE Decode(uint8_t *in, int32_t in_len, uint8_t *out, int32_t &out_len);

    virtual RET_CODE SendPacket(const AVPacket *avpkt);
    virtual RET_CODE ReceiveFrame(AVFrame *frame);
    
    virtual int GetWidth() { return ctx_->width; }
    virtual int GetHeight() { return ctx_->height; }
    virtual bool IsKeyFrame() { return bool(picture_->key_frame); }

private:
    AVCodec *codec_;
    AVCodecContext *ctx_;
    AVFrame *picture_;
    //    uint32_t		buf_len_;
    //    uint32_t 		buf_size_;
    //    uint32_t		frame_size_;
    //    uint8_t		src_;
};

#endif // H264DECODER_H
