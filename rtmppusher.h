#ifndef RTMPPUSHER_H
#define RTMPPUSHER_H

#include "librtmp/rtmp.h"
#include "mediabase.h"
#include "rtmpbase.h"
#include "naluloop.h"
#include "dlog.h"

namespace LQF
{
    class NaluLoop;
    class RTMPBase;

    enum RTMPPusherMES
    {
        RTMPPUSHER_MES_H264_DATA = 1,
        RTMPPUSHER_MES_AAC_DATA = 2
    };

    typedef struct _RTMPMetadata
    {
        // video, must be h264 type
        unsigned int nWidth;
        unsigned int nHeight;
        unsigned int nFrameRate;     // fps
        unsigned int nVideoDataRate; // bps
        unsigned int nSpsLen;
        unsigned char Sps[1024];
        unsigned int nPpsLen;
        unsigned char Pps[1024];

        // audio, must be aac type
        bool bHasAudio;
        unsigned int audio_sample_rate; //audiosamplerate
        unsigned int audio_sample_size; //audiosamplesize
        unsigned int nAudioChannels;
        char pAudioSpecCfg;
        unsigned int nAudioSpecCfgLen;

    } RTMPMetadata, *LPRTMPMetadata;

    // 实现异步发送
    class RTMPPusher : public NaluLoop, public RTMPBase
    {
        typedef RTMPBase Super;

    public:
        RTMPPusher();

        //  MetaData
        bool SendMetadata(FLVMetadataMsg *metadata);

        bool SendAudioSpecificConfig(char *data, int length);

    private:
        virtual void handle(int what, MsgBaseObj *data);
        bool sendH264SequenceHeader(VideoSequenceHeaderMsg *seq_header);
        bool sendH264Packet(char *data, int size, bool is_keyframe, unsigned int timestamp);
        //bool SendAACPacket(char *data,int size, unsigned int timestamp);
        int sendPacket(unsigned int packet_type, unsigned char *data, unsigned int size, unsigned int nTimestamp);

        int64_t time_ = 0; //在SendMetadata获取时间比较准确

        enum
        {
            FLV_CODECID_H264 = 7,
            FLV_CODECID_AAC = 10,
        };

        bool is_first_metadata_ = false; // 发送metadata
        bool is_first_video_sequence_ = false;
        bool is_first_video_frame_ = false;
        bool is_first_audio_sequence_ = false;
        bool is_first_audio_frame_ = false;
    };
} // namespace LQF
#endif // RTMPPUSHER_H
