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

bt_config_t config;
bt_requestor_t requestor;

int send_packet(int peer, data_packet_t * packet){
  #define BUFLEN 1500
  static char buf[BUFLEN];

  printf("Sending packet to %d\n", peer);
  memcpy(buf + sizeof(header_t), packet->data, sizeof(char) * packet->header.packet_len);
  packet->header.packet_len += sizeof(header_t);
  memcpy(buf, (const char *)&packet->header, sizeof(header_t));

  bt_peer_t * pinfo = bt_peer_info(&config, peer);
  if(pinfo == NULL)
    return -1;
  printf("Send!\n");
  spiffy_sendto(config.sock_fd, buf, packet->header.packet_len, 0, (struct sockaddr *) &pinfo->addr, sizeof(pinfo->addr));
  return 0;
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
  char buf[BUFLEN];
  printf("PROCESS_INBOUND_UDP SKELETON -- replace!\n");
  fromlen = sizeof(from);
  spiffy_recvfrom(sock, buf, BUFLEN, 0, (struct sockaddr *) &from, &fromlen);

  printf("PROCESS_INBOUND_UDP SKELETON -- replace!\n"
     "Incoming message from %s:%d\n%s\n\n", 
     inet_ntoa(from.sin_addr),
     ntohs(from.sin_port),
     buf);
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

  while (1) {
    int nfds;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);
    FD_SET(sock, &readfds);
    time_out.tv_sec = 1;
    time_out.tv_usec = 0;
    
    nfds = select(sock+1, &readfds, NULL, NULL, &time_out);
    
    if (nfds > 0) {
      printf("%d\n", nfds);
      if (FD_ISSET(sock, &readfds)) {
        process_inbound_udp(sock);
        printf("!!!!\n");
      }
      
      if (FD_ISSET(STDIN_FILENO, &readfds)) {
        process_user_input(STDIN_FILENO, userbuf, handle_user_input,
          "Currently unused");
        printf("......\n");
      }
    }
    printf("running\n");
  }
}
