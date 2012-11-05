#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "requestor.h"
#include "bt_parse.h"

extern bt_config_t config;

int send_whohas(bt_requestor_t * req, int chunk_id){
    data_packet_t packet;
    bt_peer_t *p;
    packet.header.magicnum = 15441;
    packet.header.version = 1;
    packet.header.packet_type = 0;
    packet.header.header_len = sizeof(header_t);
    packet.data = malloc(SHA1_HASH_SIZE * 2 * sizeof(char));
    memcpy(packet.data, req->chunks[chunk_id].hash, SHA1_HASH_SIZE * 2 * sizeof(char));
    // printf("!!%s\n", packet.data);
    for (p = config.peers; p != NULL; p = p->next) {
        packet.header.packet_len = SHA1_HASH_SIZE * 2;
        send_packet(p->id, &packet);
    }
    return 0;
}

int init_requestor(bt_requestor_t * req, char * chunkfile, char * outputfile){
    printf("Fuck %s %s\n", chunkfile, outputfile);
    FILE * fin = fopen(chunkfile, "r");
    if(fin == NULL){
        printf("Fuck %s %s\n", chunkfile, outputfile);
        return 0;
    }

    printf("!!!\n");
    int chunk_cnt = 0;
    int chunk_id;
    char chunk_hash[SHA1_HASH_SIZE * 2 + 1];
    while(fscanf(fin, "%d%s", &chunk_id, chunk_hash) == 2){
        // printf("%d %s\n", chunk_id, chunk_hash);
        // printf("......\n");
        ++ chunk_cnt;
    }
    fclose(fin);

    req->chunks = (chunk_status_t *) malloc(sizeof(chunk_status_t) * chunk_cnt);
    req->chunk_cnt = chunk_cnt;
    fin = fopen(chunkfile, "r");
    int i = 0;
    while(fscanf(fin, "%d%s", &req->chunks[i].id, chunk_hash) == 2){
        memcpy(req->chunks[i].hash, chunk_hash, sizeof(char) * SHA1_HASH_SIZE * 2 + 1);
        memset(req->chunks[i].recved, 0, sizeof(req->chunks[i].recved));
        req->chunks[i].provider_cnt = 0;
        req->chunks[i].left = BT_CHUNK_SIZE / BT_PACKET_DATA_SIZE;
        req->chunks[i].cur_provider = -1;
        send_whohas(req, i);
        ++ i;
    }

    printf("init done\n");
    req->left = chunk_cnt;
    req->in_progress = 1;
    memset(req->downloading, -1, sizeof(req->downloading));
    return 0;
}

int find_next(int * a, int n, int * b){
    int i;
    for(i=0; i<n; ++i){
        if(b[a[i]] == -1){
            return a[i];
        }
    }
    return -1;
}

int send_get(bt_requestor_t * req, int provider, int chunk_id){
    req->downloading[provider] = chunk_id;
    data_packet_t packet;
    packet.header.magicnum = 15441;
    packet.header.version = 1;
    packet.header.packet_type = 2;
    packet.header.header_len = sizeof(header_t);
    packet.header.packet_len = SHA1_HASH_SIZE * 2;
    packet.data = malloc(SHA1_HASH_SIZE * 2 * sizeof(char));
    memcpy(packet.data, req->chunks[chunk_id].hash, SHA1_HASH_SIZE * 2 * sizeof(char));
    printf("sending GET to %d\n", provider);
    send_packet(provider, &packet);
    return 0;
}

int send_ack(bt_requestor_t * req, int peer, data_packet_t * packet){
    packet->header.ack_num = packet->header.seq_num;
    packet->header.packet_type = 4;
    packet->header.packet_len = 0;
    send_packet(peer, packet);
    return 0;
}

int requstor_timeout(bt_requestor_t * req){
    int i;
    for(i=0; i<req->chunk_cnt; ++i){
        if(req->chunks[i].last_packet > 0 && (--req->chunks[i].last_packet  == 0)){
            memmove(req->chunks[i].providers + req->chunks[i].cur_provider, 
                req->chunks[i].providers + req->chunks[i].cur_provider + 1,
                (req->chunks[i].provider_cnt - req->chunks[i].cur_provider - 1) * sizeof(int));
            -- req->chunks[i].provider_cnt;
            int new_provider = find_next(req->chunks[i].providers, req->chunks[i].provider_cnt, req->downloading);
            if(new_provider != -1){
                send_get(req, new_provider, i);
            }else{
                printf("No one has this chunk %d\n", i);
                exit(1);
            }
        }
    }
    return 0;
}

int add_provider(bt_requestor_t * req, char * hash, int peer_id){
    int i, j;
    printf("Adding provider %d\n", peer_id);
    for(i=0; i<req->chunk_cnt; ++i){
        if(strncmp(req->chunks[i].hash, hash, SHA1_HASH_SIZE * 2))
            continue;
        printf("Found chunk\n");
        for(j=0; j<req->chunks[i].provider_cnt; ++j){
            if(req->chunks[i].providers[j] == peer_id)
                return 0;
        }
        printf("new provider\n");
        req->chunks[i].providers[req->chunks[i].provider_cnt] = peer_id;
        ++ req->chunks[i].provider_cnt;
        int n = find_next(req->chunks[i].providers, req->chunks[i].provider_cnt, req->downloading);
        if(n != -1){
            printf("Geting chunk\n");
            send_get(req, n ,i);
        }
    }
    return 0;
}

int finish_file(bt_requestor_t * req){
    free(req->chunks);
    req->in_progress = 0;
    printf("Finished!\n");
    return 0;
}

int finish_chunk(bt_requestor_t * req, int chunk_id){
    FILE * fout = fopen(req->outputfile, "w");
    fseek(fout, chunk_id * BT_CHUNK_SIZE, SEEK_SET);
    fwrite(req->chunks[chunk_id].data_buf, BT_CHUNK_SIZE, 1, fout);
    fclose(fout);
    if(--req->left == 0){
        finish_file(req);
    }
    return 0;
}

int update_data(bt_requestor_t * req, int peer_id, data_packet_t * packet){
    printf("Got Data! %d\n", packet->header.seq_num);
    int chunk_id = req->downloading[peer_id];
    if(chunk_id == -1)
        return 0;

    int offset = packet->header.seq_num * BT_PACKET_DATA_SIZE;
    memcpy(req->chunks[chunk_id].data_buf + offset, packet->data, BT_PACKET_DATA_SIZE * sizeof(char));
    if(req->chunks[chunk_id].recved[packet->header.seq_num] == 0){
        req->chunks[chunk_id].recved[packet->header.seq_num] = 1;
        if(-- req->chunks[chunk_id].left == 0){
            finish_chunk(req, chunk_id);
        }
    }
    req->chunks[chunk_id].last_packet = BT_DATA_TIMEOUT;
    send_ack(req, peer_id, packet);
    return 0;
}

int requestor_packet(bt_requestor_t * req, int peer_id, data_packet_t * packet){
    // IHAVE
    if(packet->header.packet_type == 1){
        int i;
        printf("!!!! %d\n", peer_id);
        for(i=0; i<packet->header.packet_len - packet->header.header_len; i+=SHA1_HASH_SIZE * 2){
            add_provider(req, packet->data + i, peer_id);
        }
    }else
    // DATA
    if(packet->header.packet_type == 3){
        update_data(req, peer_id, packet);
    }else
    // DENIED
    if(packet->header.packet_type == 5){
        printf("Denied by %d\n", peer_id);
    }
    return 0;
}
