#ifndef __PACKET_H__
#define __PACKET_H__

#include "player.h"

int packet_queue_init(PacketQueue *q);
int packet_queue_put(PacketQueue *q, AVPacket *pkt);
int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block);
int packet_queue_put_nullpacket(PacketQueue *q, int stream_index);
void packet_queue_destroy(PacketQueue *q);
void packet_queue_abort(PacketQueue *q);

#endif
