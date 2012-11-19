#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "requestor.h"
#include "bt_parse.h"

extern bt_config_t config;

int send_whohas(bt_requestor_t * req, int chunk_id){
    data_packet_t packet;
    bt_peer_t *p;
    packet.header.magicnum = BT_MAGIC;
    packet.header.version = 1;
    packet.header.packet_type = 0;
    packet.header.header_len = sizeof(header_t);
    packet.data = malloc(SHA1_HASH_SIZE * 2 * sizeof(char));
    memcpy(packet.data, req->chunks[chunk_id].hash, SHA1_HASH_SIZE * 2 * sizeof(char));

    for (p = config.peers; p != NULL; p = p->next) {
        packet.header.packet_len = SHA1_HASH_SIZE * 2;
        send_packet(p->id, &packet);
    }
    return 0;
}

int init_requestor(bt_requestor_t * req, char * chunkfile, char * outputfile){
    // clear output file
    FILE * fout = fopen(outputfile, "w");
    fclose(fout);

    FILE * fin = fopen(chunkfile, "r");
    if(fin == NULL){
        printf("Cannot find chunk file %s\n", chunkfile);
        return 0;
    }

    int chunk_cnt = 0;
    int chunk_id;
    char chunk_hash[SHA1_HASH_SIZE * 2 + 1];
    while(fscanf(fin, "%d%s", &chunk_id, chunk_hash) == 2){
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
        req->chunks[i].finished = 0;
        send_whohas(req, i);
        ++ i;
    }

    strcpy(req->outputfile, outputfile);
    req->left = chunk_cnt;
    req->in_progress = 1;
    memset(req->downloading, -1, sizeof(req->downloading));
    return 0;
}

int find_next_provider(bt_requestor_t * req, int chunk_id){
    int i = req->chunks[chunk_id].last_provider;
    int j;
    int n = req->chunks[chunk_id].provider_cnt;
    if(n == 0)
        return -1;
    for(j=0, i=(i+1)%n; j<n; ++j, i=(i+1)%n){
        int p = req->chunks[chunk_id].providers[i];
        if(req->downloading[p] == -1){
            req->chunks[chunk_id].last_provider = i;
            return p;
        }
    }
    return -1;
}

int send_get(bt_requestor_t * req, int provider, int chunk_id){
    printf("sending GET %d to %d\n", chunk_id, provider);
    if(req->downloading[provider] != -1){
        printf("!!!\n");
        return -1;
    }
    req->downloading[provider] = chunk_id;
    req->chunks[chunk_id].cur_provider = provider;
    req->chunks[chunk_id].left = BT_CHUNK_SIZE / BT_PACKET_DATA_SIZE;
    req->chunks[chunk_id].last_packet = BT_DATA_TIMEOUT;
    memset(req->chunks[chunk_id].recved, 0, sizeof(req->chunks[chunk_id].recved));
    req->chunks[chunk_id].cur_ack = 0;

    data_packet_t packet;
    packet.header.magicnum = BT_MAGIC;
    packet.header.version = 1;
    packet.header.packet_type = 2;
    packet.header.header_len = sizeof(header_t);
    packet.header.packet_len = SHA1_HASH_SIZE * 2;
    packet.data = malloc(SHA1_HASH_SIZE * 2 * sizeof(char));
    memcpy(packet.data, req->chunks[chunk_id].hash, SHA1_HASH_SIZE * 2 * sizeof(char));
    send_packet(provider, &packet);
    return 0;
}

int request_next_chunk(bt_requestor_t * req){
    int i;
    for(i=0; i<req->chunk_cnt; ++i){
        // printf("Chunk %d %d %d\n", i, req->chunks[i].finished, req->chunks[i].cur_provider);
        if(req->chunks[i].finished == 0 && req->chunks[i].cur_provider == -1){
            printf("Finding next provider for chunk %d\n", i);
            int new_provider = find_next_provider(req, i);
            printf("Get new provider %d\n", new_provider);
            if(new_provider != -1){
                send_get(req, new_provider, i);
                return 1;
            }else{
                printf("Error: chunk %d have no provider\n", i);
                return -1;
            }
        }
    }
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
    if(req->in_progress == 0)
        return 0;

    int i;
    for(i=0; i<req->chunk_cnt; ++i){
        if(req->chunks[i].finished == 0 && req->chunks[i].cur_provider != -1 && req->chunks[i].last_packet > 0 && (--req->chunks[i].last_packet  == 0)){
            printf("Time out on chunk %d from %d\n", i, req->chunks[i].cur_provider);
            req->downloading[req->chunks[i].cur_provider] = -1;
            req->chunks[i].cur_provider = -1;

            // memmove(req->chunks[i].providers + req->chunks[i].cur_provider, 
            //     req->chunks[i].providers + req->chunks[i].cur_provider + 1,
            //     (req->chunks[i].provider_cnt - req->chunks[i].cur_provider - 1) * sizeof(int));
            // -- req->chunks[i].provider_cnt;
        }
    }
    // printf("!!!\n");
    int ret = request_next_chunk(req);
    while(ret == 1){
        ret = request_next_chunk(req);
    }
    if(ret == -1){
        printf("Should abort downloading\n");
    }

    return 0;
}

int add_provider(bt_requestor_t * req, char * hash, int peer_id){
    int i, j;
    for(i=0; i<req->chunk_cnt; ++i){
        if(strncmp(req->chunks[i].hash, hash, SHA1_HASH_SIZE * 2))
            continue;
        for(j=0; j<req->chunks[i].provider_cnt; ++j){
            if(req->chunks[i].providers[j] == peer_id){
                break;
            }
        }
        if(j < req->chunks[i].provider_cnt)
            continue;
        req->chunks[i].providers[req->chunks[i].provider_cnt] = peer_id;
        ++ req->chunks[i].provider_cnt;
    }
    request_next_chunk(req);
    return 0;
}

int finish_file(bt_requestor_t * req){
    free(req->chunks);
    req->in_progress = 0;
    printf("Finished!\n");
    return 0;
}

int finish_chunk(bt_requestor_t * req, int chunk_id){
    printf("Chunk %d Finished!\n", chunk_id);
    FILE * fout = fopen(req->outputfile, "r+b");
    if(chunk_id)
        fseek(fout, chunk_id * BT_CHUNK_SIZE, SEEK_SET);
    fwrite(req->chunks[chunk_id].data_buf, BT_CHUNK_SIZE, 1, fout);
    fclose(fout);
    int provider = req->chunks[chunk_id].cur_provider;
    req->downloading[provider] = -1;
    req->chunks[chunk_id].finished = 1;
    req->chunks[chunk_id].cur_provider = -1;
    req->chunks[chunk_id].last_packet = 0;
    req->chunks[chunk_id].finished = 1;
    if(--req->left == 0){
        finish_file(req);
    }else{
        request_next_chunk(req);
    }
    return 0;
}

int update_data(bt_requestor_t * req, int peer_id, data_packet_t * packet){
    int chunk_id = req->downloading[peer_id];
    if(chunk_id == -1)
        return 0;

    req->chunks[chunk_id].last_packet = BT_DATA_TIMEOUT;
    int offset = packet->header.seq_num * BT_PACKET_DATA_SIZE;
    memcpy(req->chunks[chunk_id].data_buf + offset, packet->data, BT_PACKET_DATA_SIZE * sizeof(char));
    if(req->chunks[chunk_id].recved[packet->header.seq_num] == 0){
        req->chunks[chunk_id].recved[packet->header.seq_num] = 1;
        if(-- req->chunks[chunk_id].left == 0){
            finish_chunk(req, chunk_id);
        }
    }
    // cumulative ACK
    int seq_num = req->chunks[chunk_id].cur_ack;
    while(seq_num < BT_CHUNK_SIZE / BT_PACKET_DATA_SIZE && req->chunks[chunk_id].recved[seq_num] == 1)
        ++ seq_num;
    packet->header.seq_num = seq_num;
    send_ack(req, peer_id, packet);
    req->chunks[chunk_id].cur_ack = seq_num;
    return 0;
}

int requestor_packet(bt_requestor_t * req, int peer_id, data_packet_t * packet){
    if(req->in_progress == 0)
        return 0;
    
    // IHAVE
    if(packet->header.packet_type == 1){
        int i;
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
