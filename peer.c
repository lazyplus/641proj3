/*
 * peer.c
 *
 * Authors: Ed Bardsley <ebardsle+441@andrew.cmu.edu>,
 *          Dave Andersen
 * Class: 15-441 (Spring 2005)
 *
 * Skeleton for 15-441 Project 2.
 *
 */

#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"
#include "spiffy.h"
#include "bt_parse.h"
#include "input_buffer.h"
#include "requestor.h"
#include "responser.h"

bt_config_t config;
bt_requestor_t requestor;
bt_responser_t responser;

int send_packet(int peer, data_packet_t * packet){
  #define BUFLEN 1500
  static char buf[BUFLEN];

  // printf("Sending packet to %d\n", peer);
  // printf("Sending %d %s\n", packet->header.packet_len, packet->data);
  memcpy(buf + sizeof(header_t), packet->data, packet->header.packet_len);
  packet->header.packet_len += sizeof(header_t);
  memcpy(buf, (const char *)&packet->header, sizeof(header_t));
  // int i;
  // for(i=0; i<packet->header.packet_len; ++i){
  //   printf("%d ", buf[i]);
  // }

  bt_peer_t * pinfo = bt_peer_info(&config, peer);
  if(pinfo == NULL)
    return -1;
  // printf("Send!\n");
  spiffy_sendto(config.sock_fd, buf, packet->header.packet_len, 0, (struct sockaddr *) &pinfo->addr, sizeof(pinfo->addr));
  return 0;
}

int send_packet_cc(int peer, data_packet_t * packet){
  return send_packet(peer, packet);
}

void peer_run();

int main(int argc, char **argv) {
  bt_init(&config, argc, argv);

  DPRINTF(DEBUG_INIT, "peer.c main beginning\n");

#ifdef TESTING
  config.identity = 1; // your group number here
  strcpy(config.chunk_file, "chunkfile");
  strcpy(config.has_chunk_file, "haschunks");
#endif

  bt_parse_command_line(&config);

// #ifdef DEBUG
  // if (debug & DEBUG_INIT) {
    bt_dump_config(&config);
  // }
// #endif
  
  peer_run(&config);
  return 0;
}


void process_inbound_udp(int sock) {
  #define BUFLEN 1500
  struct sockaddr_in from;
  socklen_t fromlen;
  static char buf[BUFLEN];
  fromlen = sizeof(from);
  int ret = spiffy_recvfrom(sock, buf, BUFLEN, 0, (struct sockaddr *) &from, &fromlen);
  // printf("Got %d %d\n", ret, sizeof(header_t));
  buf[ret] = 0;
  data_packet_t packet = * (data_packet_t *) buf;
  packet.data = buf + sizeof(header_t);
  // printf("Magic %d\n", packet.header.magicnum);
  // printf("type %d\n", packet.header.packet_type);
  // printf("Data %s\n", packet.data);
  // int i;
  // for(i=sizeof(header_t); i<packet->header.packet_len; ++i){
    // printf("%d ", buf[i]);
  // }
  // printf("\n");
  // printf("Magic %d\n", packet->header.magicnum);
  // printf("Magic %d\n", packet->header.magicnum);

  bt_peer_t *p;
  for (p = config.peers; p != NULL; p = p->next) {
    char * host1, * host2;
    host1 = strdup(inet_ntoa(p->addr.sin_addr));
    host2 = strdup(inet_ntoa(from.sin_addr));
    if (strcmp(host1, host2) == 0 && p->addr.sin_port == from.sin_port) {
      free(host1);
      free(host2);
      break;
    }
    free(host1);
    free(host2);
  }

  if(p == NULL){
    printf("No such peer\n");
    return;
  }
  
  int peer_id = p->id;

  if(packet.header.packet_type == 1 || packet.header.packet_type == 3 || packet.header.packet_type == 5){
    requestor_packet(&requestor, peer_id, &packet);
  }else if(packet.header.packet_type == 0 || packet.header.packet_type == 2){
    responser_packet(&responser, peer_id, &packet);
  }else{
    //sender
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
  
  spiffy_init(config.identity, (struct sockaddr *)&myaddr, sizeof(myaddr));
  struct timeval time_out;
  int stdin_closed = 0;
  config.sock_fd = sock;

  init_responser(&responser, config.has_chunk_file, config.chunk_file);

  while (1) {
    int nfds;
    FD_ZERO(&readfds);
    if(!stdin_closed) FD_SET(STDIN_FILENO, &readfds);
    FD_SET(sock, &readfds);
    time_out.tv_sec = 1;
    time_out.tv_usec = 0;
    
    nfds = select(sock+1, &readfds, NULL, NULL, &time_out);
    
    if (nfds > 0) {
      // printf("%d\n", nfds);
      if (FD_ISSET(sock, &readfds)) {
        process_inbound_udp(sock);
      }
      
      if (FD_ISSET(STDIN_FILENO, &readfds)) {
        int ret = process_user_input(STDIN_FILENO, userbuf, handle_user_input,
          "Currently unused");
        if(ret == -1){
          stdin_closed = 1;
        }
      }
    }
    // printf("running\n");
  }
}
