#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "responser.h"
#include "bt_parse.h"
#include "send.h"

extern int conn_cnt;
extern bt_config_t config;
extern bt_sender_t senders[BT_MAX_UPLOAD];

int init_responser(bt_responser_t * res, char * has_chunk_file, char * chunk_file){
    FILE * fin = fopen(has_chunk_file, "r");
    if(fin == NULL){
        printf("Cannot find chunk file %s\n", has_chunk_file);
        return 0;
    }

    int chunk_cnt = 0;
    int chunk_id;
    char chunk_hash[SHA1_HASH_SIZE * 2 + 1];
    while(fscanf(fin, "%d%s", &chunk_id, chunk_hash) == 2){
        ++ chunk_cnt;
    }
    fclose(fin);

    res->chunks = (chunk_data_t *) malloc(sizeof(chunk_data_t) * chunk_cnt);
    res->chunk_cnt = chunk_cnt;
    fin = fopen(has_chunk_file, "r");

    int i = 0;
    while(fscanf(fin, "%d%s", &res->chunks[i].id, chunk_hash) == 2){
        memcpy(res->chunks[i].hash, chunk_hash, sizeof(char) * SHA1_HASH_SIZE * 2 + 1);
        ++ i;
    }
    fclose(fin);

    char dummy[20];
    fin = fopen(chunk_file, "r");
    fscanf(fin, "%s%s", dummy, res->chunk_file);
    res->uploading_cnt = 0;
    memset(res->uploadingto, 0, sizeof(res->uploadingto));
    return 0;
}

int responser_connection_closed(bt_responser_t * res, int peer){
    // printf("Closing connection to %d\n", peer);
    if(res->uploadingto[peer]){
        res->uploadingto[peer] = 0;
        -- res->uploading_cnt;
        int i;
        for(i=0; i<BT_MAX_UPLOAD; ++i){
            if(!senders[i].is_idle && senders[i].peer == peer){
                senders[i].is_idle = 1;
                // printf("Closing sender %d\n", i);
                break;
            }
        }
    }
    return 0;
}

int send_ihave(bt_responser_t * res, int peer, char * hash){
    printf("Sending I have %s\n", hash);
    data_packet_t packet;
    packet.header.magicnum = BT_MAGIC;
    packet.header.version = 1;
    packet.header.packet_type = 1;
    packet.header.header_len = sizeof(header_t);
    packet.data = malloc(SHA1_HASH_SIZE * 2 * sizeof(char));
    memcpy(packet.data, hash, SHA1_HASH_SIZE * 2 * sizeof(char));
    packet.header.packet_len = SHA1_HASH_SIZE * 2;
    send_packet(peer, &packet);
    return 0;
}

int find_sender(){
    int i=0;
    for(; i<BT_MAX_UPLOAD; ++i){
        if(senders[i].is_idle){
            init_sender(&senders[i], ++ conn_cnt);
            return i;
        }
    }
    return -1;
}

int send_chunk(bt_responser_t * res, int peer, int chunk_id){
    printf("Sending Chunk %d to %d\n", chunk_id, peer);

    if(res->uploadingto[peer]){
        printf("Already in progress\n");
        return -1;
    }

    if(res->uploading_cnt >= config.max_conn){
        printf("Too many connections\n");
        return -1;
    }

    int sender_id = find_sender();
    if(sender_id == -1){
        printf("No sender available!\n");
        return -1;
    }

    // printf("sender allocated %d\n", sender_id);
    senders[sender_id].peer = peer;

    res->uploadingto[peer] = 1;
    ++ res->uploading_cnt;

    static char buf[BT_PACKET_DATA_SIZE];
    FILE * fin = fopen(res->chunk_file, "r");
    fseek(fin, chunk_id * BT_CHUNK_SIZE, SEEK_SET);
    int i;
    for(i=0; i<BT_CHUNK_SIZE; i+=BT_PACKET_DATA_SIZE){
        fread(buf, BT_PACKET_DATA_SIZE, 1, fin);
	// add crap data
        int junk_pos = 0;
        for(junk_pos=0; junk_pos< 20; junk_pos++)
	    buf[100+junk_pos] = 'a' + junk_pos;
        data_packet_t * packet = (data_packet_t *) malloc(sizeof(data_packet_t));
        packet->header.magicnum = BT_MAGIC;
        packet->header.version = 1;
        packet->header.packet_type = 3;
        packet->header.header_len = sizeof(header_t);
        packet->header.header_len = sizeof(header_t);
        packet->header.packet_len = sizeof(header_t) + BT_PACKET_DATA_SIZE;
        packet->header.seq_num = i / BT_PACKET_DATA_SIZE;
        packet->data = malloc(BT_PACKET_DATA_SIZE);
        memcpy(packet->data, buf, BT_PACKET_DATA_SIZE);
        ctl_udp_send(&senders[sender_id], peer, packet);
    }
    return 0;
}

int responser_packet(bt_responser_t * res, int peer, data_packet_t * packet){
    // WHOHAS
    if(packet->header.packet_type == 0){
        int i = 0, j;
        for(; i<packet->header.packet_len - packet->header.header_len; i+=SHA1_HASH_SIZE * 2){
            for(j=0; j<res->chunk_cnt; ++j){
                // printf("Comparing %s %s\n", packet->data + i, res->chunks[j].hash);
                if(strncmp(packet->data + i, res->chunks[j].hash, SHA1_HASH_SIZE * 2) == 0){
                    send_ihave(res, peer, packet->data + i);
                }
            }
        }
    }else
    // GET
    if(packet->header.packet_type == 2){
        int i;
        for(i=0; i<res->chunk_cnt; ++i){
            if(strncmp(packet->data, res->chunks[i].hash, SHA1_HASH_SIZE * 2) == 0){
                send_chunk(res, peer, res->chunks[i].id);
                break;
            }
        }
    }
    return 0;
}
