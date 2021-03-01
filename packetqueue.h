#ifndef PACKETQUEUE_H
#define PACKETQUEUE_H

extern "C"
{
#include "libavutil/time.h"
#include "libavformat/avformat.h"
#include "SDL2/SDL.h"
#include "SDL2/SDL_thread.h"
}
#include "mediabase.h"

// 借鉴ffplay的机制
namespace LQF
{
    static AVPacket s_flush_pkt;

    typedef struct MyAVPacketList
    {
        AVPacket pkt;
        int serial;
        struct MyAVPacketList *next; // 指向下一个元素
    } MyAVPacketList;

    class PacketQueue
    {
    public:
        PacketQueue() {}
        ~PacketQueue() {}

        int packet_queue_init()
        {
            mutex = SDL_CreateMutex();
            if (!mutex)
            {
                LogError("SDL_CreateMutex(): %s\n", SDL_GetError());
                return AVERROR(ENOMEM);
            }
            cond = SDL_CreateCond();
            if (!cond)
            {
                LogError("SDL_CreateCond(): %s\n", SDL_GetError());
                return AVERROR(ENOMEM);
            }
            abort_request = 1;
            return 0;
        }

        int packet_queue_put(AVPacket *pkt)
        {
            int ret;

            SDL_LockMutex(mutex);
            ret = packet_queue_put_private(pkt);
            SDL_UnlockMutex(mutex);

            if (pkt != &s_flush_pkt && ret < 0)
                av_packet_unref(pkt);

            return ret;
        }

        int packet_queue_put_nullpacket(int stream_index)
        {
            AVPacket pkt1, *pkt = &pkt1;
            av_init_packet(pkt);
            pkt->data = NULL;
            pkt->size = 0;
            pkt->stream_index = stream_index;
            return packet_queue_put(pkt);
        }

        void packet_queue_flush()
        {
            MyAVPacketList *pkt, *pkt1;

            SDL_LockMutex(mutex);
            for (pkt = first_pkt; pkt; pkt = pkt1)
            {
                pkt1 = pkt->next;
                av_packet_unref(&pkt->pkt);
                av_freep(&pkt);
            }
            last_pkt = NULL;
            first_pkt = NULL;
            nb_packets = 0;
            size = 0;
            duration = 0;
            SDL_UnlockMutex(mutex);
        }

        void packet_queue_destroy()
        {
            packet_queue_flush();
            SDL_DestroyMutex(mutex);
            SDL_DestroyCond(cond);
        }

        void packet_queue_abort()
        {
            SDL_LockMutex(mutex);

            abort_request = 1;

            SDL_CondSignal(cond);

            SDL_UnlockMutex(mutex);
        }

        void packet_queue_start()
        {
            SDL_LockMutex(mutex);
            abort_request = 0;
            //        packet_queue_put_private(&s_flush_pkt);
            SDL_UnlockMutex(mutex);
        }

        /* return < 0 if aborted, 0 if no packet and > 0 if packet.  */
        int packet_queue_get(AVPacket *pkt, int block, int *serial)
        {
            MyAVPacketList *pkt1;
            int ret;

            SDL_LockMutex(mutex);

            for (;;)
            {
                if (abort_request)
                {
                    ret = -1;
                    break;
                }

                pkt1 = first_pkt;
                if (pkt1)
                {
                    first_pkt = pkt1->next;
                    if (!first_pkt)
                        last_pkt = NULL;
                    nb_packets--;
                    size -= pkt1->pkt.size + sizeof(*pkt1);
                    duration -= pkt1->pkt.duration;
                    *pkt = pkt1->pkt;
                    if (serial)
                        *serial = pkt1->serial;
                    av_free(pkt1);
                    ret = 1;
                    break;
                }
                else if (!block)
                {
                    ret = 0;
                    break;
                }
                else
                {
                    SDL_CondWait(cond, mutex);
                }
            }
            SDL_UnlockMutex(mutex);
            return ret;
        }
        
        inline int get_nb_packets()
        {
            return nb_packets;
        }
        inline int64_t get_duration()
        {
            int64_t temp_duration = 0;
            SDL_LockMutex(mutex);
            temp_duration = duration;
            SDL_UnlockMutex(mutex);
            return temp_duration;
        }

    private:
        int packet_queue_put_private(AVPacket *pkt)
        {
            MyAVPacketList *pkt1;

            if (abort_request)
                return -1;

            pkt1 = (MyAVPacketList *)av_malloc(sizeof(MyAVPacketList));
            if (!pkt1)
                return -1;
            pkt1->pkt = *pkt;
            pkt1->next = NULL;
            if (pkt == &s_flush_pkt)
            {
                serial++;
                printf("serial = %d\n", serial++);
            }
            pkt1->serial = serial;

            if (!last_pkt)
                first_pkt = pkt1;
            else
                last_pkt->next = pkt1;
            last_pkt = pkt1;
            nb_packets++;
            size += pkt1->pkt.size + sizeof(*pkt1);
            duration += pkt1->pkt.duration;
            /* XXX: should duplicate packet data in DV case */
            SDL_CondSignal(cond);
            return 0;
        }

        MyAVPacketList *first_pkt = NULL; // 队首
        MyAVPacketList *last_pkt = NULL;  // 队尾

        int nb_packets = 0;   // 包数量(队列大小)
        int size = 0;         // 所有元素的大小总和
        int64_t duration = 0; // 所有元素的播放持续时间

        int abort_request = 0; // 用户退出请求标志
        int serial = 0;

        SDL_mutex *mutex = NULL; // 线程安全
        SDL_cond *cond = NULL;   // 用于读, 写线程相互通知
    };
}

#endif // PACKETQUEUE_H
