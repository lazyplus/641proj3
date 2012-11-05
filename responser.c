#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "responser.h"

int init_responser(bt_responser_t * res, char * has_chunk_file, char * chunk_file){
    FILE * fin = fopen(has_chunk_file, "r");
    if(fin == NULL){
        printf("Fuck %s\n", has_chunk_file);
        return 0;
    }

    char dummy[20];
    // fscanf(fin, "%s%s", dummy, res->chunk_file);
    // fscanf(fin, "%s", dummy);

    // printf("!!!\n");
    int chunk_cnt = 0;
    int chunk_id;
    char chunk_hash[SHA1_HASH_SIZE * 2 + 1];
    while(fscanf(fin, "%d%s", &chunk_id, chunk_hash) == 2){
        printf("%d %s\n", chunk_id, chunk_hash);
        printf("......\n");
        ++ chunk_cnt;
    }
    fclose(fin);

    res->chunks = (chunk_data_t *) malloc(sizeof(chunk_data_t) * chunk_cnt);
    res->chunk_cnt = chunk_cnt;
    fin = fopen(has_chunk_file, "r");
    // fscanf(fin, "%s%s", dummy, res->chunk_file);
    // fscanf(fin, "%s", dummy);

    int i = 0;
    while(fscanf(fin, "%d%s", &res->chunks[i].id, chunk_hash) == 2){
        memcpy(res->chunks[i].hash, chunk_hash, sizeof(char) * SHA1_HASH_SIZE * 2 + 1);
        ++ i;
    }
    fclose(fin);

    fin = fopen(chunk_file, "r");
    fscanf(fin, "%s%s", dummy, res->chunk_file);
    // fscanf(fin, "%s", dummy);
    res->uploading_cnt = 0;
    memset(res->uploading, 0, sizeof(res->uploading));
    // strcpy(res->chunk_file, chunk_file);
    return 0;
}

int connection_closed(bt_responser_t * res, int peer);

int send_ihave(bt_responser_t * res, int peer_id, char * hash){
    printf("Sending I have %s\n", hash);
    data_packet_t packet;
    packet.header.magicnum = 15441;
    packet.header.version = 1;
    packet.header.packet_type = 1;
    packet.header.header_len = sizeof(header_t);
    packet.data = malloc(SHA1_HASH_SIZE * 2 * sizeof(char));
    memcpy(packet.data, hash, SHA1_HASH_SIZE * 2 * sizeof(char));
    packet.header.packet_len = SHA1_HASH_SIZE * 2;
    send_packet(peer_id, &packet);
    return 0;
}

int send_chunk(bt_responser_t * res, int peer_id, int chunk_id){
    printf("Sending Chunk %d to %d\n", chunk_id, peer_id);
    static char buf[BT_PACKET_DATA_SIZE];
    FILE * fin = fopen(res->chunk_file, "r");
    printf("Reading chunk file %s\n", res->chunk_file);
    fseek(fin, chunk_id * BT_CHUNK_SIZE, SEEK_SET);
    int i;
    for(i=0; i<BT_CHUNK_SIZE; i+=BT_PACKET_DATA_SIZE){
        fread(buf, BT_PACKET_DATA_SIZE, 1, fin);
        data_packet_t * packet = (data_packet_t *) malloc(sizeof(data_packet_t));
        packet->header.magicnum = 15441;
        packet->header.version = 1;
        packet->header.packet_type = 3;
        packet->header.header_len = sizeof(header_t);
        packet->header.header_len = sizeof(header_t);
        packet->header.packet_len = sizeof(header_t) + BT_PACKET_DATA_SIZE;
        packet->header.seq_num = i / BT_PACKET_DATA_SIZE;
        packet->data = malloc(BT_PACKET_DATA_SIZE);
        memcpy(packet->data, buf, BT_PACKET_DATA_SIZE);
        send_packet_cc(peer_id, packet);
        struct timespec wait_time;
        wait_time.tv_sec = 0;
        wait_time.tv_nsec = 1000000;
        nanosleep(&wait_time, NULL);
    }
    return 0;
}

int responser_packet(bt_responser_t * res, int peer_id, data_packet_t * packet){
    // WHOHAS
    if(packet->header.packet_type == 0){
        int i = 0, j;
        for(; i<packet->header.packet_len - packet->header.header_len; i+=SHA1_HASH_SIZE * 2){
            for(j=0; j<res->chunk_cnt; ++j){
                printf("Comparing %s %s\n", packet->data + i, res->chunks[j].hash);
                if(strncmp(packet->data + i, res->chunks[j].hash, SHA1_HASH_SIZE * 2) == 0){
                    send_ihave(res, peer_id, packet->data + i);
                }
            }
        }
    }else
    // GET
    if(packet->header.packet_type == 2){
        int i;
        for(i=0; i<res->chunk_cnt; ++i){
            if(strncmp(packet->data, res->chunks[i].hash, SHA1_HASH_SIZE * 2) == 0){
                send_chunk(res, peer_id, res->chunks[i].id);
                break;
            }
        }
    }
    return 0;
}
