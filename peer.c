#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "debug.h"
#include "spiffy.h"
#include "bt_parse.h"
#include "input_buffer.h"
#include "requestor.h"
#include "responser.h"
#include "send.h"

bt_config_t config;
bt_requestor_t requestor;
bt_responser_t responser;
bt_sender_t senders[BT_MAX_UPLOAD];
int conn_cnt = 0;
FILE * window_size_log;

long start_time = 0;

long my_get_time(){
    struct timeval cur_time;
    gettimeofday(&cur_time, NULL);
    return cur_time.tv_sec * 1000000 + cur_time.tv_usec - start_time;
}

void init_my_timer(){
    start_time = 0;
    start_time = my_get_time();
}

int free_packet(data_packet_t * packet){
    free(packet->data);
    free(packet);
    return 0;
}

int send_packet(int peer, data_packet_t * packet){
    static char buf[BT_MAX_PKT_SIZE];

    memcpy(buf + sizeof(header_t), packet->data, packet->header.packet_len);
    packet->header.packet_len += sizeof(header_t);
    memcpy(buf, (const char *)&packet->header, sizeof(header_t));

    bt_peer_t * pinfo = bt_peer_info(&config, peer);
    if(pinfo == NULL)
        return -1;

    spiffy_sendto(config.sock_fd, buf, packet->header.packet_len, 0, (struct sockaddr *) &pinfo->addr, sizeof(pinfo->addr));
    return 0;
}

int connection_closed(int peer){
    return responser_connection_closed(&responser, peer);
}

void peer_run();

int main(int argc, char **argv) {
    bt_init(&config, argc, argv);
    bt_parse_command_line(&config);
    // bt_dump_config(&config);

    peer_run(&config);
    return 0;
}

void process_inbound_udp(int sock) {
    static char buf[BT_MAX_PKT_SIZE];
    struct sockaddr_in from;
    socklen_t fromlen = sizeof(from);
    
    int ret = spiffy_recvfrom(sock, buf, BT_MAX_PKT_SIZE, 0, (struct sockaddr *) &from, &fromlen);
    buf[ret] = 0;
    data_packet_t packet = * (data_packet_t *) buf;
    packet.data = buf + sizeof(header_t);

    if(packet.header.magicnum != BT_MAGIC){
        printf("Ignoring unrecognizable packet\n");
        return;
    }

    bt_peer_t *p;
    for (p = config.peers; p != NULL; p = p->next) {
        if (!memcmp(&p->addr.sin_addr, &from.sin_addr, sizeof(struct in_addr))
             && p->addr.sin_port == from.sin_port) {
            break;
        }
    }

    if(p == NULL){
        printf("Ignoring packet from unknown peer\n");
        return;
    }
    
    int peer = p->id;

    if(packet.header.packet_type == 1 || packet.header.packet_type == 3 || packet.header.packet_type == 5){
        requestor_packet(&requestor, peer, &packet);
    }else if(packet.header.packet_type == 0 || packet.header.packet_type == 2){
        responser_packet(&responser, peer, &packet);
    }else{
        int i;
        for(i=0; i<BT_MAX_UPLOAD; ++i){
            if(!senders[i].is_idle && senders[i].peer == peer){
                ctl_udp_ack(&senders[i], peer, &packet);
            }
        }
    }
}

void process_get(char *chunkfile, char *outputfile) {
    if (requestor.in_progress) {
        printf("Waiting for previous downloading, try later\n");
        return;
    }

    init_requestor(&requestor, chunkfile, outputfile);
}

void handle_user_input(char *line, void *cbdata) {
    char chunkf[128], outf[128];

    bzero(chunkf, sizeof(chunkf));
    bzero(outf, sizeof(outf));

    if (sscanf(line, "GET %120s %120s", chunkf, outf)) {
        if (strlen(outf) > 0) {
            process_get(chunkf, outf);
        }
    }
}

void peer_run() {
    init_my_timer();

    int sock;
    struct sockaddr_in myaddr;
    fd_set readfds;
    struct user_iobuf *userbuf;
    
    if ((userbuf = create_userbuf()) == NULL) {
        perror("peer_run could not allocate userbuf");
        exit(-1);
    }
    
    if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP)) == -1) {
        perror("peer_run could not create socket");
        exit(-1);
    }
    
    bzero(&myaddr, sizeof(myaddr));
    myaddr.sin_family = AF_INET;
    myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    myaddr.sin_port = htons(config.myport);
    
    if (bind(sock, (struct sockaddr *) &myaddr, sizeof(myaddr)) == -1) {
        perror("peer_run could not bind socket");
        exit(-1);
    }
    
    struct timeval time_out;
    int stdin_closed = 0;
    config.sock_fd = sock;

    bt_peer_t *p = bt_peer_info(&config, config.identity);
    spiffy_init(config.identity, (struct sockaddr *)&(p->addr), sizeof(struct sockaddr_in));

    init_responser(&responser, config.has_chunk_file, config.chunk_file);
    int i;
    for(i=0; i<BT_MAX_UPLOAD; ++i){
        senders[i].is_idle = 1;
    }

    //open window size change log
    window_size_log = fopen("problem2-peer.txt", "w+");

    long last_time = my_get_time();

    while (1) {
        int nfds;
        FD_ZERO(&readfds);
        if(!stdin_closed) FD_SET(STDIN_FILENO, &readfds);
        FD_SET(sock, &readfds);
        time_out.tv_sec = 1;
        time_out.tv_usec = 0;
        
        nfds = select(sock+1, &readfds, NULL, NULL, &time_out);

        long cur_time = my_get_time();
        if(cur_time - last_time > TICKS_PER_MILISECOND * 1000){
            last_time = cur_time;
            requstor_timeout(&requestor);
            for(i=0; i<BT_MAX_UPLOAD; ++i){
                if(!senders[i].is_idle)
                    ctl_udp_time_out(&senders[i]);
            }
        }
        
        if (nfds > 0) {
            if (FD_ISSET(sock, &readfds)) {
                process_inbound_udp(sock);
            }
            
            if (FD_ISSET(STDIN_FILENO, &readfds)) {
                int ret = process_user_input(STDIN_FILENO, userbuf, handle_user_input, "Currently unused");
                if(ret == -1){
                    stdin_closed = 1;
                }
            }
        }
    }
}
