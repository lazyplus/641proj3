#ifndef SEND_H

#define SEND_H

typedef struct packet_pointer{
    data_packet_t* data_packet;
    int ack;
    int peer;
}packet_pointer_t;

struct bt_sender_s{
    int head, tail;
    packet_pointer_t pointer_array[4*1024];
    int window_size;
};

#endif
