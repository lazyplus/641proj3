#ifndef RESPONSER_H

#define RESPONSER_H

#include "peer.h"
#include "sha.h"

struct chunk_data_s{
    int id;
    char hash[SHA1_HASH_SIZE * 2 + 1];
};
typedef struct chunk_data_s chunk_data_t;

struct responser_s{
    char chunk_file[BT_FILENAME_LEN];
    int chunk_cnt;
    chunk_data_t * chunks;
    int uploading_cnt;
    char uploading[BT_MAX_PEERS];
};
typedef struct responser_s bt_responser_t;

int init_responser(bt_responser_t * res, char * has_chunk_file, char * chunk_file);
int responser_packet(bt_responser_t * res, int peer_id, data_packet_t * packet);
int connection_closed(bt_responser_t * res, int peer);

#endif
