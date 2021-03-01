/**
 * 本程序使用 RTMP_ReadPacket()与 RTMP_ClientPacket()方式读取流数据, 接收RTMP流媒体并存储视频h264和音频aac文件.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "librtmp/rtmp_sys.h"
#include "librtmp/log.h"

#define FLV_PULL_TIME (230 * 1000) // 录制时间
#define FLV_PULL_FRAME 200         // 至少录制的帧数

static int InitSockets()
{
#ifdef WIN32
    WORD version;
    WSADATA wsaData;
    version = MAKEWORD(1, 1);
    return (WSAStartup(version, &wsaData) == 0);
#endif
}

static void CleanupSockets()
{
#ifdef WIN32
    WSACleanup();
#endif
}

/**
* @brief get_millisecond
* @return 返回毫秒
*/
static int64_t getCurrentTimeMsec()
{
#ifdef _WIN32
    return (int64_t)GetTickCount();
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return ((int64_t)tv.tv_sec * 1000 + (unsigned long long)tv.tv_usec / 1000);
#endif
}

int rtmpPull(int argc, char *argv[])
{
    InitSockets(); // 主要是针对Windows

    //is live stream ?
    bool bLiveStream = true;
    RTMP *rtmp = NULL;
    FILE *fp = NULL;
    int64_t connect_time = 0;
    //    char url[] = "rtmp://202.69.69.180:443/webcast/bshdlive-pc";
    //       char url[] = "rtmp://111.229.231.225/live/livestream?token=liaoqingfu";
    char url[] = "rtmp://192.168.1.11/live/livestream";

    // 设置librtmp的log级别
    RTMP_LogLevel loglvl = RTMP_LOGDEBUG;
    RTMP_LogSetLevel(loglvl);

    //直播
    fp = fopen("receive.flv", "wb");
    if (!fp)
    {
        RTMP_LogInfo(RTMP_LOGERROR, "Open File LogError.\n");
        CleanupSockets();
        return -1;
    }

    rtmp = RTMP_Alloc();
    // 初始化了一下RTMP*rtmp变量的成员
    RTMP_LogInfo(RTMP_LOGINFO, "RTMP_Init ...\n");
    RTMP_Init(rtmp);

    //这个参数实际配置的是接收数据的超时时间, 而不是连接服务器的超时时间
    // 如果想修改connect超时的时间则需要自己修改源码
    // 如果想设置发送数据的超时时间，也要自己修改源码
    rtmp->Link.timeout = 5;

    // 函数将rtmp源地址的端口，app，等url参数进行解析，设置到rtmp变量中。
    // 比如这样的地址： rtmp://host[:port]/path swfUrl=url tcUrl=url
    RTMP_LogInfo(RTMP_LOGINFO, "RTMP_SetupURL ...\n");
    if (!RTMP_SetupURL(rtmp, url))
    {
        RTMP_LogInfo(RTMP_LOGERROR, "SetupURL Err\n");
        RTMP_Free(rtmp);
        CleanupSockets();
        return -1;
    }

    if (bLiveStream)
    {
        rtmp->Link.lFlags |= RTMP_LF_LIVE;
    }

    RTMP_LogInfo(RTMP_LOGINFO, "RTMP_SetBufferMS ...\n");
    // 在实际使用时该参数意义不大
    RTMP_SetBufferMS(rtmp, 200 * 1000);
    connect_time = getCurrentTimeMsec();
    // 真正开始连接服务器, 完成了连接的建立，一级RTMP协议层的应用握手
    RTMP_LogInfo(RTMP_LOGINFO, "RTMP_Connect ...\n");
    if (!RTMP_Connect(rtmp, NULL))
    {
        RTMP_LogInfo(RTMP_LOGERROR, "Connect Err, timeout:%ldms\n", getCurrentTimeMsec() - connect_time);
        RTMP_Free(rtmp);
        CleanupSockets();
        return -1;
    }
    RTMP_LogInfo(RTMP_LOGINFO, "RTMP_ConnectStream ...\n");
    // 完成了一个流的创建，以及打开，触发服务端发送数据过来，返回后，服务端或者客户端就可以开始发送数据了
    if (!RTMP_ConnectStream(rtmp, NULL))
    {
        RTMP_LogInfo(RTMP_LOGERROR, "ConnectStream Err\n");
        RTMP_Close(rtmp);
        RTMP_Free(rtmp);
        CleanupSockets();
        return -1;
    }

    size_t writeByte = 0;
    size_t readByte = 0;
    size_t readCount = 0;
    int frameCount = 0;
    int64_t currentTime = 0;
    int64_t startTime = 0;
    int bufsize = 1024 * 1024 * 2;
    char *buf = (char *)malloc(bufsize);
    memset(buf, 0, bufsize);

    RTMP_LogInfo(RTMP_LOGINFO, "\nRTMP_Read ....\n");
    startTime = getCurrentTimeMsec();
    // RTMP_Read第一个包读出来的是FLV header 和Metadata tag
    // RTMP_Read主要用来dump flv
    while (readByte = RTMP_Read(rtmp, buf, bufsize))
    {
        writeByte = fwrite(buf, 1, readByte, fp);
        if (writeByte != readByte)
        {
            RTMP_LogInfo(RTMP_LOGERROR, "fwrite failed, readByte:%d, writeByte:%d \n",
                         readByte, writeByte);
        }

        readCount += readByte;
        frameCount++;

        if (frameCount % 100 == 0)
        {
            currentTime = getCurrentTimeMsec();
            RTMP_LogPrintf("T-[%lld]，count-[%d],Receive: %5dByte, Total: %5.2fkB\n",
                           currentTime - startTime, frameCount,
                           readByte, readCount * 1.0 / 1024);
            if ((currentTime - startTime > FLV_PULL_TIME) && (frameCount > FLV_PULL_FRAME))
            {
                break;
            }
        }
    }

    if (buf)
        free(buf);

    if (fp)
        fclose(fp);

    if (rtmp)
    {
        RTMP_Close(rtmp);
        RTMP_Free(rtmp);
        CleanupSockets();
        rtmp = NULL;
    }

    printf("\nfinish\n");

    return 0;
}
