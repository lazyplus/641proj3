#ifndef PEER_H

#define PEER_H

#include <sys/types.h>

#define BT_PACKET_DATA_SIZE 512
#define BT_FILENAME_LEN 255
#define BT_CHUNK_SIZE (512 * 1024)
#define BT_MAX_PEERS 1024

typedef struct header_s {
  short magicnum;
  char version;
  char packet_type;
  short header_len;
  short packet_len; 
  u_int seq_num;
  u_int ack_num;
} header_t;  

typedef struct data_packet {
  header_t header;
  char * data;
} data_packet_t;


int send_packet(int peer, data_packet_t * packet);

#endif
