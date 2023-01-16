#include "packet.h"

int packet_queue_init(PacketQueue *q)
{
    memset(q, 0, sizeof(PacketQueue));

    q->mutex = SDL_CreateMutex();
    if (!q->mutex)
    {
        printf("SDL_CreateMutex(): %s\n", SDL_GetError());
        return AVERROR(ENOMEM);
    }

    q->cond = SDL_CreateCond();
    if (!q->cond)
    {
        printf("SDL_CreateCond(): %s\n", SDL_GetError());
        return AVERROR(ENOMEM);
    }

    q->abort_request = 0;

    return 0;
}

// 写队列尾部。pkt是一包还未解码的音频数据
int packet_queue_put(PacketQueue *q, AVPacket *pkt)
{
    AVPacketToken *token;

    if (av_packet_make_refcounted(pkt) < 0)
    {
        printf("[pkt] is not refrence counted\n");
        return -1;
    }

    token = av_malloc(sizeof(AVPacketToken));
    if (!token)
    {
        return -1;
    }

    token->pkt = *pkt;
    token->next = NULL;

    SDL_LockMutex(q->mutex);

    if (!q->last) // 队列为空
    {
        q->first = token;
    }
    else
    {
        q->last->next = token;
    }

    q->last = token;
    q->nb_packets++;
    q->size += token->pkt.size;

    // 发个条件变量的信号：重启等待q->cond条件变量的一个线程
    SDL_CondSignal(q->cond);

    SDL_UnlockMutex(q->mutex);

    return 0;
}

// 读队列头部。
int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block)
{
    AVPacketToken *token;
    int ret;

    SDL_LockMutex(q->mutex);

    while (1)
    {
        token = q->first;

        if (token) // 队列非空，取一个出来
        {
            q->first = token->next;
            if (!q->first)
            {
                q->last = NULL;
            }
            
            q->nb_packets--;
            q->size -= token->pkt.size;
            *pkt = token->pkt;

            av_free(token);
            ret = 1;
            
            break;
        }
        else if (!block) // 队列空且阻塞标志无效，则立即退出
        {
            ret = 0;
            break;
        }
        else // 队列空且阻塞标志有效，则等待
        {
            SDL_CondWait(q->cond, q->mutex);
        }
    }

    SDL_UnlockMutex(q->mutex);

    return ret;
}

int packet_queue_put_nullpacket(PacketQueue *q, int stream_index)
{
    AVPacket pkt1, *pkt = &pkt1;
    
    // av_init_packet(pkt);
    pkt->data = NULL;
    pkt->size = 0;
    
    pkt->stream_index = stream_index;

    return packet_queue_put(q, pkt);
}

void packet_queue_flush(PacketQueue *q)
{
    AVPacketToken *pkt, *pkt1;

    SDL_LockMutex(q->mutex);

    for (pkt = q->first; pkt; pkt = pkt1)
    {
        pkt1 = pkt->next;

        if (pkt->pkt.size)
        {
            av_packet_unref(&pkt->pkt);
        }
        
        av_freep(&pkt);
    }

    q->last = NULL;
    q->first = NULL;
    q->nb_packets = 0;
    q->size = 0;
    q->duration = 0;

    SDL_UnlockMutex(q->mutex);
}

void packet_queue_destroy(PacketQueue *q)
{
    packet_queue_flush(q);
    SDL_DestroyMutex(q->mutex);
    SDL_DestroyCond(q->cond);
}

void packet_queue_abort(PacketQueue *q)
{
    SDL_LockMutex(q->mutex);

    q->abort_request = 1;

    SDL_CondSignal(q->cond);

    SDL_UnlockMutex(q->mutex);
}
