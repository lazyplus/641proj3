#ifndef REQUESTOR_H

#define REQUESTOR_H

#include "peer.h"
#include "sha.h"

#define BT_DATA_TIMEOUT 5

struct chunk_status_s{
    int last_provider;
    int last_packet;
    char data_buf[BT_CHUNK_SIZE];
    int left;
    char recved[BT_CHUNK_SIZE / BT_PACKET_DATA_SIZE];
    int provider_cnt;
    int providers[BT_MAX_PEERS];
    int cur_provider;
    int finished;
    char hash[SHA1_HASH_SIZE * 2 + 1];
    int id;
};

typedef struct chunk_status_s chunk_status_t;

struct bt_requestor_s{
    int in_progress;
    int downloading[BT_MAX_PEERS];
    char outputfile[BT_FILENAME_LEN];
    char chunkfile[BT_FILENAME_LEN];
    int chunk_cnt;
    chunk_status_t * chunks;
    int left;
};

typedef struct bt_requestor_s bt_requestor_t;

int init_requestor(bt_requestor_t * req, char * chunkfile, char * outputfile);
int requstor_timeout(bt_requestor_t * req);
int requestor_packet(bt_requestor_t * req, int peer_id, data_packet_t * packet);

#endif
