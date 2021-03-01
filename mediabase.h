#ifndef _CONFIG_H_
#define _CONFIG_H_
#include <cstdlib>
#include <cstddef> // size_t
#include <cstdint>
#include <map>
#include <vector>
#include <sstream>
#include <functional>
#include <string>
#include <cstring>
#ifndef _MSC_VER
#include <strings.h>
#endif
#include <ctype.h>

#include "librtmp/rtmp.h"

#include "dlog.h"

enum CB_EVENT
{
    // 音频事件
    EVT_AUD_PKT_CACHE_ENOUGH, //
    EVT_AUD_UNDER_RUN,        // 音频缺乏数据输出
    // 视频事件

    // RTMP拉流事件

};

#ifdef _MSC_VER
static inline int strcasecmp(const char *s1, const char *s2)
{
    //   while  (toupper((unsigned char)*s1) == toupper((unsigned char)*s2++))
    //       if (*s1++ == '\0') return 0;
    //   return(toupper((unsigned char)*s1) - toupper((unsigned char)*--s2));
    while ((unsigned char)*s1 == (unsigned char)*s2++)
        if (*s1++ == '\0')
            return 0;
    return ((unsigned char)*s1 - (unsigned char)*--s2);
}

#endif

#define QCIF 0     // 176  x 144 	AR:	1,222222222
#define CIF 1      // 352  x 288	AR:	1,222222222
#define VGA 2      // 640  x 480	AR:	1,333333333
#define PAL 3      // 768  x 576	AR:	1,333333333
#define HVGA 4     // 480  x 320	AR:	1,5
#define QVGA 5     // 320  x 240	AR:	1,333333333
#define HD720P 6   // 1280 x 720	AR:	1,777777778
#define WQVGA 7    // 400  x 240	AR:	1,666666667
#define W448P 8    // 768  x 448	AR:	1,714285714
#define SD448P 9   // 576  x 448	AR:	1,285714286
#define W288P 10   // 512  x 288	AR:	1,777777778
#define W576 11    // 1024 x 576	AR:	1,777777778
#define FOURCIF 12 // 704  x 576	AR:	1,222222222
#define FOURSIF 13 // 704  x 480	AR:	1,466666667
#define XGA 14     // 1024 x 768	AR:	1,333333333
#define WVGA 15    // 800  x 480	AR:	1,666666667
#define DCIF 16    // 528  x 384	AR:	1,375
#define SIF 17     // 352  x 240	AR:	1,466666667
#define QSIF 18    // 176  x 120	AR:	1,466666667
#define SD480P 19  // 480  x 360	AR:	1,333333333
#define SQCIF 20   // 128  x 96	AR:	1,333333333
#define SCIF 21    // 256  x 192	AR:	1,333333333
#define HD1080P 22 // 1920 x 1080  AR:     1,777777778
#define UW720P 23  // 1680 x 720   AR:	2,333333333

inline uint32_t GetWidth(uint32_t size)
{
    //Depending on size
    switch (size)
    {
    case QCIF:
        return 176;
    case CIF:
        return 352;
    case VGA:
        return 640;
    case PAL:
        return 768;
    case HVGA:
        return 480;
    case QVGA:
        return 320;
    case HD720P:
        return 1280;
    case WQVGA:
        return 400;
    case W448P:
        return 768;
    case SD448P:
        return 576;
    case W288P:
        return 512;
    case W576:
        return 1024;
    case FOURCIF:
        return 704;
    case FOURSIF:
        return 704;
    case XGA:
        return 1024;
    case WVGA:
        return 800;
    case DCIF:
        return 528;
    case SIF:
        return 352;
    case QSIF:
        return 176;
    case SD480P:
        return 480;
    case SQCIF:
        return 128;
    case SCIF:
        return 256;
    case HD1080P:
        return 1920;
    case UW720P:
        return 1680;
    }
    //Nothing
    return 0;
}

inline uint32_t GetHeight(uint32_t size)
{
    //Depending on size
    switch (size)
    {
    case QCIF:
        return 144;
    case CIF:
        return 288;
    case VGA:
        return 480;
    case PAL:
        return 576;
    case HVGA:
        return 320;
    case QVGA:
        return 240;
    case HD720P:
        return 720;
    case WQVGA:
        return 240;
    case W448P:
        return 448;
    case SD448P:
        return 448;
    case W288P:
        return 288;
    case W576:
        return 576;
    case FOURCIF:
        return 576;
    case FOURSIF:
        return 480;
    case XGA:
        return 768;
    case WVGA:
        return 480;
    case DCIF:
        return 384;
    case SIF:
        return 240;
    case QSIF:
        return 120;
    case SD480P:
        return 360;
    case SQCIF:
        return 96;
    case SCIF:
        return 192;
    case HD1080P:
        return 1080;
    case UW720P:
        return 720;
    }
    //Nothing
    return 0;
}

class Properties : public std::map<std::string, std::string>
{
public:
    bool HasProperty(const std::string &key) const
    {
        return find(key) != end();
    }

    void SetProperty(const char *key, int intval)
    {
        SetProperty(std::string(key), std::to_string(intval));
    }

    void SetProperty(const char *key, uint32_t val)
    {
        SetProperty(std::string(key), std::to_string(val));
    }

    void SetProperty(const char *key, uint64_t val)
    {
        SetProperty(std::string(key), std::to_string(val));
    }

    void SetProperty(const char *key, const char *val)
    {
        SetProperty(std::string(key), std::string(val));
    }

    void SetProperty(const std::string &key, const std::string &val)
    {
        insert(std::pair<std::string, std::string>(key, val));
    }

    void GetChildren(const std::string &path, Properties &children) const
    {
        //Create sarch string
        std::string parent(path);
        //Add the final .
        parent += ".";
        //For each property
        for (const_iterator it = begin(); it != end(); ++it)
        {
            const std::string &key = it->first;
            //Check if it is from parent
            if (key.compare(0, parent.length(), parent) == 0)
                //INsert it
                children.SetProperty(key.substr(parent.length(), key.length() - parent.length()), it->second);
        }
    }

    void GetChildren(const char *path, Properties &children) const
    {
        GetChildren(std::string(path), children);
    }

    Properties GetChildren(const std::string &path) const
    {
        Properties properties;
        //Get them
        GetChildren(path, properties);
        //Return
        return properties;
    }

    Properties GetChildren(const char *path) const
    {
        Properties properties;
        //Get them
        GetChildren(path, properties);
        //Return
        return properties;
    }

    void GetChildrenArray(const char *path, std::vector<Properties> &array) const
    {
        //Create sarch string
        std::string parent(path);
        //Add the final .
        parent += ".";

        //Get array length
        int length = GetProperty(parent + "length", 0);

        //For each element
        for (int i = 0; i < length; ++i)
        {
            char index[64];
            //Print string
            snprintf(index, sizeof(index), "%d", i);
            //And get children
            array.push_back(GetChildren(parent + index));
        }
    }

    const char *GetProperty(const char *key) const
    {
        return GetProperty(key, "");
    }

    std::string GetProperty(const char *key, const std::string defaultValue) const
    {
        //Find item
        const_iterator it = find(std::string(key));
        //If not found
        if (it == end())
            //return default
            return defaultValue;
        //Return value
        return it->second;
    }

    std::string GetProperty(const std::string &key, const std::string defaultValue) const
    {
        //Find item
        const_iterator it = find(key);
        //If not found
        if (it == end())
            //return default
            return defaultValue;
        //Return value
        return it->second;
    }

    const char *GetProperty(const char *key, const char *defaultValue) const
    {
        //Find item
        const_iterator it = find(std::string(key));
        //If not found
        if (it == end())
            //return default
            return defaultValue;
        //Return value
        return it->second.c_str();
    }

    const char *GetProperty(const std::string &key, char *defaultValue) const
    {
        //Find item
        const_iterator it = find(key);
        //If not found
        if (it == end())
            //return default
            return defaultValue;
        //Return value
        return it->second.c_str();
    }

    int GetProperty(const char *key, int defaultValue) const
    {
        return GetProperty(std::string(key), defaultValue);
    }

    int GetProperty(const std::string &key, int defaultValue) const
    {
        //Find item
        const_iterator it = find(key);
        //If not found
        if (it == end())
            //return default
            return defaultValue;
        //Return value
        return atoi(it->second.c_str());
    }

    uint64_t GetProperty(const char *key, uint64_t defaultValue) const
    {
        return GetProperty(std::string(key), defaultValue);
    }

    uint64_t GetProperty(const std::string &key, uint64_t defaultValue) const
    {
        //Find item
        const_iterator it = find(key);
        //If not found
        if (it == end())
            //return default
            return defaultValue;
        //Return value
        return atoll(it->second.c_str());
    }

    bool GetProperty(const char *key, bool defaultValue) const
    {
        return GetProperty(std::string(key), defaultValue);
    }

    bool GetProperty(const std::string &key, bool defaultValue) const
    {
        //Find item
        const_iterator it = find(key);
        //If not found
        if (it == end())
            //return default
            return defaultValue;
        //Get value
        char *val = (char *)it->second.c_str();
        //Check it
        if (strcasecmp(val, (char *)"yes") == 0)
            return true;
        else if (strcasecmp(val, (char *)"true") == 0)
            return true;
        //Return value
        return (atoi(val));
    }
};

inline void *malloc32(size_t size)
{
    void *ptr = malloc(size);
    //    if(posix_memalign(&ptr,32,size))
    //        return NULL;
    return ptr;
}

class ByteBuffer
{
public:
    ByteBuffer()
    {
        //Set buffer size
        size = 0;
        //Allocate memory
        buffer = NULL;
        //NO length
        length = 0;
    }

    ByteBuffer(const uint32_t size)
    {
        //NO length
        length = 0;
        //Calculate new size
        this->size = size;
        //Realloc
        buffer = (uint8_t *)malloc32(size);
    }

    ByteBuffer(const uint8_t *data, const uint32_t size)
    {
        //Calculate new size
        this->size = size;
        //Realloc
        buffer = (uint8_t *)malloc32(size);
        //Copy
        memcpy(buffer, data, size);
        //Increase length
        length = size;
    }

    ByteBuffer(const ByteBuffer *bytes)
    {
        //Calculate new size
        size = bytes->GetLength();
        //Realloc
        buffer = (uint8_t *)malloc32(size);
        //Copy
        memcpy(buffer, bytes->GetData(), size);
        //Increase length
        length = size;
    }

    ByteBuffer(const ByteBuffer &bytes)
    {
        //Calculate new size
        size = bytes.GetLength();
        //Realloc
        buffer = (uint8_t *)malloc32(size);
        //Copy
        memcpy(buffer, bytes.GetData(), size);
        //Increase length
        length = size;
    }

    ByteBuffer *Clone() const
    {
        return new ByteBuffer(buffer, length);
    }

    virtual ~ByteBuffer()
    {
        //Clear memory
        if (buffer)
            free(buffer);
    }

    void Alloc(const uint32_t size)
    {
        //Calculate new size
        this->size = size;
        //Realloc
        buffer = (uint8_t *)realloc(buffer, size);
    }

    void Set(const uint8_t *data, const uint32_t size)
    {
        //Check size
        if (size > this->size)
            //Allocate new size
            Alloc(size * 3 / 2);
        //Copy
        memcpy(buffer, data, size);
        //Increase length
        length = size;
    }

    uint32_t Append(const uint8_t *data, const uint32_t size)
    {
        uint32_t pos = length;
        //Check size
        if (size + length > this->size)
            //Allocate new size
            Alloc((size + length) * 3 / 2);
        //Copy
        memcpy(buffer + length, data, size);
        //Increase length
        length += size;
        //Return previous pos
        return pos;
    }

    const uint8_t *GetData() const
    {
        return buffer;
    }

    uint32_t GetSize() const
    {
        return size;
    }

    uint32_t GetLength() const
    {
        return length;
    }

protected:
    uint8_t *buffer;
    uint32_t length;
    uint32_t size;
};

enum RET_CODE
{
    RET_ERR_UNKNOWN = -2,       // 未知错误
    RET_FAIL = -1,              // 失败
    RET_OK = 0,                 // 正常
    RET_ERR_OPEN_FILE,          // 打开文件失败
    RET_ERR_NOT_SUPPORT,        // 不支持
    RET_ERR_OUTOFMEMORY,        // 没有内存
    RET_ERR_STACKOVERFLOW,      // 溢出
    RET_ERR_NULLREFERENCE,      // 空参考
    RET_ERR_ARGUMENTOUTOFRANGE, //
    RET_ERR_PARAMISMATCH,       //
    RET_ERR_MISMATCH_CODE,      // 没有匹配的编解码器
    RET_ERR_EAGAIN,
    RET_ERR_EOF
};

//post 消息对象基类
class MsgBaseObj
{
public:
    MsgBaseObj() {}
    virtual ~MsgBaseObj() {}
};

struct LooperMessage;
typedef struct LooperMessage LooperMessage;

// 消息载体
struct LooperMessage
{
    int what;
    MsgBaseObj *obj;
    bool quit;
};

enum RTMP_BODY_TYPE
{
    RTMP_BODY_METADATA,  // metadata
    RTMP_BODY_AUD_RAW,   // 纯raw data
    RTMP_BODY_AUD_SPEC,  // AudioSpecificConfig
    RTMP_BODY_VID_RAW,   // raw data
    RTMP_BODY_VID_CONFIG // H264Configuration
};
class YUVStruct : public MsgBaseObj
{
public:
    int size = 0;
    int width = 0;
    int height = 0;
    char *data = NULL;
    YUVStruct(int size, int width, int height);
    YUVStruct(char *data, int size, int width, int height);
    virtual ~YUVStruct();
};
//NV12: YYYYYYYY UVUV
typedef YUVStruct YUVNV12;
//NV21: YYYYYYYY VUVU
typedef YUVStruct YUVNV21;

class YUV420p : public YUVStruct
{
public:
    char *Y;
    char *U;
    char *V;

    YUV420p(int32_t size, int32_t width, int32_t height);
    YUV420p(char *data, int32_t size, int32_t width, int32_t height);
    virtual ~YUV420p();
};

class FLVMetadataMsg : public MsgBaseObj
{
public:
    FLVMetadataMsg() {}
    virtual ~FLVMetadataMsg() {}

    bool has_audio = false;
    bool has_video = false;
    int audiocodeid = -1;
    int audiodatarate = 0;
    int audiodelay = 0;
    int audiosamplerate = 0;
    int audiosamplesize = 0;
    int channles;

    bool canSeekToEnd = 0;

    std::string creationdate;
    int duration = 0;
    int64_t filesize = 0;
    double framerate = 0;
    int height = 0;
    bool stereo = true;

    int videocodecid = -1;
    int64_t videodatarate = 0;
    int width = 0;
};

class AudioRawMsg : public MsgBaseObj
{
public:
    AudioRawMsg(int size, int with_adts = 0)
    {
        this->size = size;
        type = 0;
        with_adts_ = with_adts;
        data = (unsigned char *)malloc(size * sizeof(char));
    }
    AudioRawMsg(const unsigned char *buf, int bufLen, int with_adts = 0)
    {
        this->size = bufLen;
        type = buf[4] & 0x1f;
        with_adts_ = with_adts;
        data = (unsigned char *)malloc(bufLen * sizeof(char));
        memcpy(data, buf, bufLen);
    }

    virtual ~AudioRawMsg()
    {
        if (data)
            free(data);
    }
    int type;
    int size;
    int with_adts_ = 0;
    unsigned char *data = NULL;
    uint32_t pts;
};

class AudioSpecMsg : public MsgBaseObj
{
public:
    AudioSpecMsg(uint8_t profile, uint8_t channel_num, uint32_t samplerate)
    {
        profile_ = profile;
        channels_ = channel_num;
        sample_rate_ = samplerate;
    }

    virtual ~AudioSpecMsg() {}
    uint8_t profile_ = 2; //2 : AAC LC(Low Complexity)
    uint8_t channels_ = 2;
    uint32_t sample_rate_ = 48000;
};

class NaluStruct : public MsgBaseObj
{
public:
    NaluStruct(int size);

    NaluStruct(const unsigned char *buf, int bufLen);
    virtual ~NaluStruct();
    int type;
    int size;
    unsigned char *data = NULL;
    uint32_t pts;
};

class VideoSequenceHeaderMsg : public MsgBaseObj
{
public:
    VideoSequenceHeaderMsg(uint8_t *sps, int sps_size, uint8_t *pps, int pps_size)
    {
        sps_ = (uint8_t *)malloc(sps_size * sizeof(uint8_t));
        pps_ = (uint8_t *)malloc(pps_size * sizeof(uint8_t));
        if (!sps_ || !pps_)
        {
            LogError("VideoSequenceHeaderMsg malloc failed");
            return;
        }
        sps_size_ = sps_size;
        memcpy(sps_, sps, sps_size);
        pps_size_ = pps_size;
        memcpy(pps_, pps, pps_size);
    }

    virtual ~VideoSequenceHeaderMsg()
    {
        if (sps_)
            free(sps_);
        if (pps_)
            free(pps_);
    }
    uint8_t *sps_;
    int sps_size_;
    uint8_t *pps_;
    int pps_size_;
    unsigned int nWidth;
    unsigned int nHeight;
    unsigned int nFrameRate;     // fps
    unsigned int nVideoDataRate; // bps
};

struct MsgRTMPPPack : MsgBaseObj
{
    RTMPPacket *rtmpPack = NULL;
    MsgRTMPPPack(RTMPPacket &pack)
    {
        rtmpPack = (RTMPPacket *)malloc(sizeof(RTMPPacket));
        memcpy(rtmpPack, &pack, sizeof(RTMPPacket));
    }
    virtual ~MsgRTMPPPack()
    {
        if (rtmpPack)
        {
            RTMPPacket_Free(rtmpPack);
            rtmpPack = NULL;
        }
    }
};

#endif
