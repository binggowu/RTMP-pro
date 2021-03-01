#ifndef RTMPPACKAGER_H
#define RTMPPACKAGER_H
#include "librtmp/rtmp.h"
namespace LQF {

enum RTMPChannel
{
   RTMP_NETWORK_CHANNEL = 2,   ///< channel for network-related messages (bandwidth report, ping, etc)
   RTMP_SYSTEM_CHANNEL,        ///< channel for sending server control messages
   RTMP_AUDIO_CHANNEL,         ///< channel for audio data
   RTMP_VIDEO_CHANNEL   = 6,   ///< channel for video data
   RTMP_SOURCE_CHANNEL  = 8,   ///< channel for a/v invokes
};
class RTMPPackager
{
public:
    virtual void Pack(RTMPPacket *packet, char* buf, const char* data, int length) const = 0;
    virtual void Metadata(RTMPPacket *packet, char* buf, const char* data, int length) const = 0;
};
}


#endif // RTMPPACKAGER_H
