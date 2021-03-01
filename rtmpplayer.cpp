extern "C"
{
#include "libavformat/avformat.h"
}
#include "librtmp/rtmp_sys.h"
#include "dlog.h"
#include "timeutil.h"
#include "avtimebase.h"
#include "mediabase.h"
#include "rtmpplayer.h"

namespace LQF
{
    RTMPPlayer::RTMPPlayer() : RTMPBase(RTMP_BASE_TYPE_PLAY)
    {
    }

    RTMPPlayer::~RTMPPlayer()
    {
        if (worker_)
        {
            Stop();
        }
    }

    void RTMPPlayer::parseScriptTag(RTMPPacket &packet)
    {
        LogInfo("begin parse info %d", packet.m_nBodySize);
        AMFObject obj;
        AVal val;
        AMFObjectProperty *property;
        AMFObject subObject;
        if (AMF_Decode(&obj, packet.m_body, packet.m_nBodySize, FALSE) < 0)
        {
            LogInfo("%s, error decoding invoke packet", __FUNCTION__);
        }
        AMF_Dump(&obj);
        LogInfo(" amf obj %d", obj.o_num);
        for (int n = 0; n < obj.o_num; n++)
        {
            property = AMF_GetProp(&obj, NULL, n);
            if (property != NULL)
            {
                if (property->p_type == AMF_OBJECT)
                {
                    AMFProp_GetObject(property, &subObject);
                    for (int m = 0; m < subObject.o_num; m++)
                    {
                        property = AMF_GetProp(&subObject, NULL, m);
                        LogInfo("val = %s", property->p_name.av_val);
                        if (property != NULL)
                        {
                            if (property->p_type == AMF_OBJECT)
                            {
                            }
                            else if (property->p_type == AMF_BOOLEAN)
                            {
                                int bVal = AMFProp_GetBoolean(property);
                                if (strncasecmp("stereo", property->p_name.av_val, property->p_name.av_len) == 0)
                                {
                                    audio_channel = bVal > 0 ? 2 : 1;
                                    LogInfo("parse channel %d", audio_channel);
                                }
                            }
                            else if (property->p_type == AMF_NUMBER)
                            {
                                double dVal = AMFProp_GetNumber(property);
                                if (strncasecmp("width", property->p_name.av_val, property->p_name.av_len) == 0)
                                {
                                    video_width = (int)dVal;
                                    LogInfo("parse widht %d", video_width);
                                }
                                else if (strcasecmp("height", property->p_name.av_val) == 0)
                                {
                                    video_height = (int)dVal;
                                    LogInfo("parse Height %d", video_height);
                                }
                                else if (strcasecmp("framerate", property->p_name.av_val) == 0)
                                {
                                    video_frame_rate = (int)dVal;
                                    LogInfo("parse frame_rate %d", video_frame_rate);
                                    if (video_frame_rate > 0)
                                    {
                                        video_frame_duration_ = 1000 / video_frame_rate;
                                    }
                                }
                                else if (strcasecmp("videocodecid", property->p_name.av_val) == 0)
                                {
                                    video_codec_id = (int)dVal;
                                    LogInfo("parse video_codec_id %d", video_codec_id);
                                }
                                else if (strcasecmp("audiosamplerate", property->p_name.av_val) == 0)
                                {
                                    audio_sample_rate = (int)dVal;
                                    LogInfo("parse audiosamplerate %d", audio_sample_rate);
                                }
                                else if (strcasecmp("audiodatarate", property->p_name.av_val) == 0)
                                {
                                    audio_bit_rate = (int)dVal;
                                    LogInfo("parse audiodatarate %d", audio_bit_rate);
                                }
                                else if (strcasecmp("audiosamplesize", property->p_name.av_val) == 0)
                                {
                                    audio_sample_size = (int)dVal;
                                    LogInfo("parse audiosamplesize %d", audio_sample_size);
                                }
                                else if (strcasecmp("audiocodecid", property->p_name.av_val) == 0)
                                {
                                    audio_codec_id = (int)dVal;
                                    LogInfo("parse audiocodecid %d", audio_codec_id);
                                }
                                else if (strcasecmp("filesize", property->p_name.av_val) == 0)
                                {
                                    file_size = (int)dVal;
                                    LogInfo("parse filesize %d", file_size);
                                }
                            }
                            else if (property->p_type == AMF_STRING)
                            {
                                AMFProp_GetString(property, &val);
                            }
                        }
                    }
                }
                else
                {
                    AMFProp_GetString(property, &val);

                    LogInfo("val = %s", val.av_val);
                }
            }
        }
    }

    // 创建线程
    RET_CODE RTMPPlayer::Start()
    {
        if (!worker_)
        {
            worker_ = new std::thread(std::bind(&RTMPPlayer::readPacketThread, this));
            if (worker_ == NULL)
            {
                LogError("new std::thread failed");
                return RET_FAIL;
            }
        }
        return RET_OK;
    }

    // 退出线程
    void RTMPPlayer::Stop()
    {
        request_exit_thread_ = true;
        if (worker_)
        {
            if (worker_->joinable())
            {
                worker_->join();
            }
            delete worker_;
            worker_ = NULL;
        }
    }

    // 线程入口函数, 解析收到的RTMP chunk, 并如video/audio解码线程的PacketQueue.
    void *RTMPPlayer::readPacketThread()
    {
        AVPlayTime *play_time = AVPlayTime::GetInstance();
        RTMPPacket packet = {0};
        int64_t cur_time = TimesUtil::GetTimeMillisecond();
        int64_t pre_time = cur_time;
        while (!request_exit_thread_)
        {
            // 断线重连
            if (!IsConnect())
            {
                LogInfo("断线重连");
                if (!Connect(url_)) //重连失败
                {
                    LogInfo("短线重连 reConnect fail %s", url_.c_str());
                    msleep(10);
                    continue;
                }
            }

            cur_time = TimesUtil::GetTimeMillisecond();
            int64_t t = cur_time - pre_time;
            pre_time = cur_time;
            RTMP_ReadPacket(rtmp_, &packet);
            int64_t diff = TimesUtil::GetTimeMillisecond() - cur_time;
            // if (t > 10 || diff > 10)
            //     LogInfo("cur-pre t:%ld, ReadPacket:%ld", t, diff);

            if (RTMPPacket_IsReady(&packet)) // 检测是不是整个包组好了
            {
                diff = TimesUtil::GetTimeMillisecond() - cur_time;
                if (diff > 10)
                {
                    bool keyframe = false;
                    if (packet.m_packetType == RTMP_PACKET_TYPE_VIDEO)
                    {
                        keyframe = 0x17 == packet.m_body[0] ? true : false; // ???
                    }
                    // LogInfo("RTMPPacket_IsReady:%ld, keyframe:%d, type:%u,size:%u",
                    //         diff, keyframe, packet.m_packetType,
                    //         packet.m_nBodySize);
                }
                uint8_t nalu_header[4] = {0x00, 0x00, 0x00, 0x01};
                // Process packet, eg: set chunk size, set bw, ...
                // RTMP_ClientPacket(m_pRtmp, &packet);
                if (!packet.m_nBodySize)
                    continue;
                if (packet.m_packetType == RTMP_PACKET_TYPE_VIDEO)
                {
                    // 解析完数据再发送给解码器
                    // 判断起始字节, 检测是不是spec config, 还原出sps pps等
                    // 重组帧
                    bool keyframe = 0x17 == packet.m_body[0] ? true : false;
                    bool sequence = 0x00 == packet.m_body[1];
                    // if (keyframe)
                    // {
                    //     LogInfo("keyframe=%s, sequence=%s, size:%d", keyframe ? "true" : "false",
                    //             sequence ? "true" : "false",
                    //             packet.m_nBodySize);
                    // }

                    // SPS/PPS sequence
                    if (keyframe && sequence)
                    {
                        LogInfo("%s:%s:t:%u", play_time->getRtmpTag(), play_time->getAvcHeaderTag(), play_time->getCurrenTime());

                        is_got_video_sequence_ = true;

                        // 解析SPS
                        uint32_t offset = 10;                              // ???
                        uint32_t sps_num = packet.m_body[offset++] & 0x1f; // ???
                        if (sps_num > 0)
                        {
                            sps_vector_.clear(); // 先清空原来的缓存
                        }
                        for (int i = 0; i < sps_num; i++)
                        {
                            uint8_t ch0 = packet.m_body[offset];
                            uint8_t ch1 = packet.m_body[offset + 1];
                            uint32_t sps_len = ((ch0 << 8) | ch1); // ???
                            offset += 2;

                            // Write sps data
                            std::string sps;
                            sps.append(nalu_header, nalu_header + 4); // 存储 start code
                            sps.append(packet.m_body + offset, packet.m_body + offset + sps_len);
                            sps_vector_.push_back(sps);
                            offset += sps_len;
                        }

                        // 解析PPS
                        uint32_t pps_num = packet.m_body[offset++] & 0x1f; // ???
                        if (pps_num > 0)
                        {
                            pps_vector_.clear(); // 先清空原来的缓存
                        }
                        for (int i = 0; i < pps_num; i++)
                        {
                            uint8_t ch0 = packet.m_body[offset];
                            uint8_t ch1 = packet.m_body[offset + 1];
                            uint32_t pps_len = ((ch0 << 8) | ch1);
                            offset += 2;
                            // Write pps data
                            std::string pps;
                            pps.append(nalu_header, nalu_header + 4); // 存储 start code
                            pps.append(packet.m_body + offset, packet.m_body + offset + pps_len);
                            pps_vector_.push_back(pps);
                            offset += pps_len;
                        }

                        // 封装SPS成AVPacket
                        AVPacket sps_pkt = {0};
                        sps_pkt.size = sps_vector_[0].size();
                        sps_pkt.data = (uint8_t *)av_malloc(sps_pkt.size);
                        if (av_packet_from_data(&sps_pkt, sps_pkt.data, sps_pkt.size) == 0)
                        {
                            memcpy(sps_pkt.data, (uint8_t *)sps_vector_[0].c_str(), sps_vector_[0].size());
                            video_packet_callable_object_(&sps_pkt); // 入video解码线程的PacketQueue
                        }
                        else
                        {
                            LogError("av_packet_from_data sps_pkt failed");
                        }

                        // 封装PPS成AVPacket
                        AVPacket pps_pkt = {0};
                        pps_pkt.size = pps_vector_[0].size();
                        pps_pkt.data = (uint8_t *)av_malloc(pps_pkt.size);
                        if (av_packet_from_data(&pps_pkt, pps_pkt.data, pps_pkt.size) == 0)
                        {
                            memcpy(pps_pkt.data, (uint8_t *)pps_vector_[0].c_str(), pps_vector_[0].size());
                            video_packet_callable_object_(&pps_pkt); // 入video解码线程的PacketQueue
                        }
                        else
                        {
                            LogError("av_packet_from_data pps_pkt failed");
                        }

                        firt_entry = true;
                    }
                    // Nalu frames
                    else if (firt_entry)
                    {
                        // Debug
                        if (keyframe && !is_got_video_iframe_)
                        {
                            is_got_video_iframe_ = true;
                            LogInfo("%s:%s:t:%u", play_time->getRtmpTag(), play_time->getAvcIFrameTag(), play_time->getCurrenTime());
                        }

                        // Debug. 打印前PRINT_MAX_FRAMES帧的时间信息，包括i帧
                        if (got_video_frames_++ < PRINT_MAX_FRAMES)
                        {
                            LogInfo("%s:%s:k:%d:t:%u", play_time->getRtmpTag(), play_time->getAvcFrameTag(), keyframe, play_time->getCurrenTime());
                        }

                        uint32_t duration = video_frame_duration_;

                        // 计算pts
                        if (video_pre_pts_ == -1)
                        {
                            video_pre_pts_ = packet.m_nTimeStamp;
                            if (!packet.m_hasAbsTimestamp)
                            {
                                LogWarn("no init video pts");
                            }
                        }
                        else
                        {
                            if (packet.m_hasAbsTimestamp)
                            {
                                video_pre_pts_ = packet.m_nTimeStamp;
                            }
                            else
                            {
                                duration = packet.m_nTimeStamp;
                                video_pre_pts_ += packet.m_nTimeStamp;
                            }
                        }
                        LogDebug("vpts:%u, t:%u", video_pre_pts_, packet.m_nTimeStamp);

                        uint32_t offset = 5; // ???
                        while (offset < packet.m_nBodySize)
                        {
                            uint8_t ch0 = packet.m_body[offset];
                            uint8_t ch1 = packet.m_body[offset + 1];
                            uint8_t ch2 = packet.m_body[offset + 2];
                            uint8_t ch3 = packet.m_body[offset + 3];
                            uint32_t data_len = ((ch0 << 24) | (ch1 << 16) | (ch2 << 8) | ch3); // ???
                            memcpy(&packet.m_body[offset], nalu_header, 4);
                            offset += 4; // 跳过data_len占用的4字节

                            uint8_t nalu_type = 0x1f & packet.m_body[4];
                            // LogError("nalu_type:%d", nalu_type);
                            // if (nalu_type <= 0x5                      // sei
                            //     || nalu_type == 8 || nalu_type == 16) // 单元分隔符
                            {
                                AVPacket nalu_pkt = {0};
                                nalu_pkt.size = data_len + 4;
                                nalu_pkt.data = (uint8_t *)av_malloc(nalu_pkt.size);

                                if (av_packet_from_data(&nalu_pkt, nalu_pkt.data, nalu_pkt.size) == 0)
                                {
                                    memcpy(&nalu_pkt.data[0], nalu_header, 4);
                                    memcpy(&nalu_pkt.data[4], (uint8_t *)&packet.m_body[offset], data_len);

                                    nalu_pkt.duration = duration;
                                    nalu_pkt.dts = video_pre_pts_;
                                    if (keyframe)
                                        nalu_pkt.flags = AV_PKT_FLAG_KEY;

                                    video_packet_callable_object_(&nalu_pkt); // 入video解码线程的PacketQueue
                                }
                                else
                                {
                                    LogError("av_packet_from_data nalu_pkt failed");
                                }
                            }
                            offset += data_len; // 跳过data_len
                        }
                    }
                    else
                    {
                        LogWarn("unhandle data");
                    }
                }
                else if (packet.m_packetType == RTMP_PACKET_TYPE_AUDIO)
                {
                    static int64_t s_is_pre_ready = TimesUtil::GetTimeMillisecond();
                    cur_time = TimesUtil::GetTimeMillisecond();
                    // LogInfo("aud ready  t:%ld", cur_time - s_is_pre_ready);
                    s_is_pre_ready = cur_time;

                    bool sequence = (0x00 == packet.m_body[1]); // ???
                    // LogInfo("sequence=%s\n", sequence ? "true" : "false");
                    uint8_t format = 0, samplerate = 0, sampledepth = 0, type = 0;
                    uint8_t frame_length_flag = 0, depend_on_core_coder = 0, extension_flag = 0;
                    // AAC sequence
                    if (sequence)
                    {
                        LogInfo("%s:%s:t:%u", play_time->getRtmpTag(),
                                play_time->getAacHeaderTag(),
                                play_time->getCurrenTime());

                        format = (packet.m_body[0] & 0xf0) >> 4;
                        samplerate = (packet.m_body[0] & 0x0c) >> 2;
                        sampledepth = (packet.m_body[0] & 0x02) >> 1;
                        type = packet.m_body[0] & 0x01;
                        // AAC(AudioSpecificConfig)
                        if (format == 10)
                        {
                            // AAC格式
                            uint8_t ch0 = packet.m_body[2];
                            uint8_t ch1 = packet.m_body[3];
                            uint16_t config = ((ch0 << 8) | ch1);
                            profile_ = (config & 0xF800) >> 11;
                            sample_frequency_index_ = (config & 0x0780) >> 7;
                            channels_ = (config & 0x78) >> 3;
                            frame_length_flag = (config & 0x04) >> 2;
                            depend_on_core_coder = (config & 0x02) >> 1;
                            extension_flag = config & 0x01;
                        }
                        // Speex(Fix data here, so no need to parse...)
                        else if (format == 11)
                        {
                            // MP3格式: 16 KHz, mono, 16bit/sample
                            type = 0;
                            sampledepth = 1;
                            samplerate = 4;
                        }
                        audio_sample_rate = RTMPBase::GetSampleRateByFreqIdx(sample_frequency_index_);
                        AudioSpecMsg *aud_spec_msg = new AudioSpecMsg(profile_,
                                                                      channels_,
                                                                      audio_sample_rate);
                        audio_callable_object_(RTMP_BODY_AUD_SPEC, aud_spec_msg, false);
                    }
                    // Audio frames
                    else
                    {
                        if (got_audio_frames_++ < PRINT_MAX_FRAMES)
                        {
                            // 打印前PRINT_MAX_FRAMES帧的时间信息，包括i帧
                            LogInfo("%s:%s:t:%u", play_time->getRtmpTag(),
                                    play_time->getAacDataTag(),
                                    play_time->getCurrenTime());
                        }

                        // 解析pts
                        uint32_t duration = audio_frame_duration_;
                        if (audio_pre_pts_ == -1)
                        {
                            audio_pre_pts_ = packet.m_nTimeStamp;
                            if (!packet.m_hasAbsTimestamp)
                            {
                                LogWarn("no init video pts");
                            }
                        }
                        else
                        {
                            if (packet.m_hasAbsTimestamp)
                                audio_pre_pts_ = packet.m_nTimeStamp;
                            else
                            {
                                duration = packet.m_nTimeStamp;
                                audio_pre_pts_ += packet.m_nTimeStamp;
                            }
                        }
                        LogDebug("apts:%u, t:%u", audio_pre_pts_, packet.m_nTimeStamp);

                        // ADTS(7 bytes) + AAC data
                        uint32_t data_len = packet.m_nBodySize - 2 + 7;
                        uint8_t adts[7];
                        adts[0] = 0xff;
                        adts[1] = 0xf9;
                        adts[2] = ((profile_ - 1) << 6) | (sample_frequency_index_ << 2) | (channels_ >> 2);
                        adts[3] = ((channels_ & 3) << 6) + (data_len >> 11);
                        adts[4] = (data_len & 0x7FF) >> 3;
                        adts[5] = ((data_len & 7) << 5) + 0x1F;
                        adts[6] = 0xfc;

                        AVPacket aac_pkt = {0};
                        aac_pkt.size = data_len;
                        aac_pkt.data = (uint8_t *)av_malloc(aac_pkt.size);
                        if (av_packet_from_data(&aac_pkt, aac_pkt.data, aac_pkt.size) == 0)
                        {
                            // 带 adts header
                            memcpy(&aac_pkt.data[0], adts, 7);
                            memcpy(&aac_pkt.data[7], packet.m_body + 2, packet.m_nBodySize - 2);
                            aac_pkt.duration = duration;
                            aac_pkt.dts = audio_pre_pts_;
                            // aac_pkt.pts = audio_pre_pts_;
                            audio_packet_callable_object_(&aac_pkt); // 如AudioDecoderLoop的PacketQueue

                            static FILE *rtmp_dump_aac = NULL;
                            if (!rtmp_dump_aac)
                            {
                                rtmp_dump_aac = fopen("rtmp.aac", "wb+");
                            }
                            fwrite(aac_pkt.data, aac_pkt.size, 1, rtmp_dump_aac);
                            fflush(rtmp_dump_aac);
                        }
                        else
                        {
                            LogError("av_packet_from_data aac_pkt failed");
                        }
                    }
                    // LogInfo("aud finish  t:%ld\n", TimesUtil::GetTimeMillisecond() - cur_time);
                }
                else if (packet.m_packetType == RTMP_PACKET_TYPE_INFO)
                {
                    // LogInfo("onReadVideoAndAudioInfo ");
                    LogInfo("%s:%s:t:%u", play_time->getRtmpTag(),
                            play_time->getMetadataTag(),
                            play_time->getCurrenTime());
                    is_got_metadta_ = true;
                    parseScriptTag(packet);
                    if (video_width > 0 && video_height > 0)
                    {
                        FLVMetadataMsg *metadata = new FLVMetadataMsg();
                        metadata->width = video_width;   //720;
                        metadata->height = video_height; //480;
                        video_callable_object_(RTMP_BODY_METADATA, metadata, false);
                    }
                }
                else
                {
                    LogInfo("can't handle it ");
                    RTMP_ClientPacket(rtmp_, &packet);
                }
                // RTMP_ClientPacket(m_pRtmp, &packet);
                RTMPPacket_Free(&packet);

                memset(&packet, 0, sizeof(RTMPPacket));
            }
        }
        LogInfo("thread exit");
        return NULL;
    }

}
