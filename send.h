#ifndef SEND_H

#define SEND_H
#include "peer.h"

#define BUFFER_LEN (BT_CHUNK_SIZE / BT_PACKET_DATA_SIZE)
#define START_WINDOW_SIZE 64
#define DUP_ACK_THRES 3
#define ACK_TIMEOUT (CLOCKS_PER_SEC * 2)
#define BT_RTT_ALPHA (0.9)

typedef struct packet_pointer{
    data_packet_t* data;
    int ack;
    int sent_ts;
}packet_pointer_t;

typedef struct bt_sender{
    int is_idle;
    int id;
    int peer;
    double rtt;
    int window_size;
    int last_ack_num;
    int last_ack_cnt;
    int head, tail;
    packet_pointer_t pkt_buf[BUFFER_LEN];
}bt_sender_t;

void init_sender(bt_sender_t *sender, int id);
int ctl_udp_send(bt_sender_t *sender, int peer, data_packet_t *new_packet);
int ctl_udp_ack(bt_sender_t *sender, int peer, data_packet_t *new_packet);
int ctl_udp_time_out(bt_sender_t *sender);

int wd_lost(bt_sender_t * sender);
int wd_ack(bt_sender_t * sender);

#endif
