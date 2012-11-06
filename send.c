#include <stdio.h>
#include "peer.h"
#include "send.h"

void init_sender(struct bt_sender_s *sender){
    sender->head = 0;
    sender->tail = sender->head;
    sender->window_size = 8;
}

int ctl_udp_send(struct bt_sender_s *sender, int peer, data_packet_t *new_packet){
    static int first_time = 1;
    int *head = &(sender->head);
    int *tail = &(sender->tail);
    int *window_size = &(sender->window_size);

    /*if(head == NULL && tail == NULL){
        // first time called
        struct packet_pointer *new_node, *last_node;
        int i;
        // creat a circle linked list
        head = (struct packet_pointer*)malloc(sizeof(struct packet_pointer));
        last_node = head;
        for(i=1;i<1024*4;i++){
            new_node = (struct packet_pointer*)malloc(sizeof(struct packet_pointer));
            last_node->next_node = new_node;
            last_node = new_node;
        }
        new_node->next_node = head;
        head->data_packet
        tail = head;
    }*/
    if(new_packet == NULL){
        printf("Error: Empty data pointer in upd send\n");
        //exit(EXIT_FAILURE);
        return -1;
    }
   
    if(first_time){
        sender->pointer_array[*head].data_packet = new_packet;
        sender->pointer_array[*head].ack = 0;
        sender->pointer_array[*head].peer = peer;
        send_packet(peer, sender->pointer_array[*head].data_packet);
        first_time = 0;
        ++ *tail;
    }else{
        // if((*tail+1)%(4*1024) != *head){
        sender->pointer_array[*tail].data_packet = new_packet;
        sender->pointer_array[*tail].ack = 0;
        sender->pointer_array[*tail].peer = peer;
        if((*tail + 4*1024 - *head)%(4*1024) < *window_size){
            send_packet(peer, sender->pointer_array[*tail].data_packet);
        }
        *tail = (*tail+1)%(4*1024);
        // }else{
            // array is full
            // printf("Error: array is full\n");
            // return -1;
        // }
    }
    return 0;
}

int ctl_udp_ack(struct bt_sender_s *sender, int peer, data_packet_t *new_packet){
    int i, j;
    int offset=0;
    int *head = &(sender->head);
    int *tail = &(sender->tail);
    int *window_size = &(sender->window_size);
    int old_window;
    int data_cnt;
    int tmp = (*tail + 4*1024 - *head) % (4*1024);

    data_cnt = (tmp >(*window_size))?*window_size:tmp;

    // mark the ACK for the packet
    for(i = *head, j=0; j < data_cnt; i=(i+1)%(4*1024), j++){
        if((sender->pointer_array[i].peer == peer)&&
          (sender->pointer_array[i].data_packet->header.seq_num == new_packet->header.ack_num)){
            sender->pointer_array[i].ack = 1;      
        }
        break;
    }
    
    // check for continuous ACKs
    for(i = *head, j=0; j< data_cnt; i=(i+1)%(4*1024), j++){
        if(sender->pointer_array[i].ack == 1){
            offset++;
            if(sender->pointer_array[i].data_packet->header.seq_num == BT_CHUNK_SIZE / BT_PACKET_DATA_SIZE - 1){
                connection_closed(sender->pointer_array[i].peer);
            }
        }else{
            break;
        }
    }
    
    // slide window if offset > 0
    if(offset > 0){
        for(i = *head, j=0; j< offset; i=(i+1)%(4*1024), j++){
            sender->pointer_array[i].peer = -1;
            sender->pointer_array[i].ack = 0;
            free_packet(sender->pointer_array[i].data_packet);
        }
        old_window = (*head + *window_size)%(4*1024);
        *head = (*head + offset)%(4*1024);
        // send out new packets sliding into the window
        for(i= old_window, j = 0; j < offset; i=(i+1)%(4*1024), j++){
            if(sender->pointer_array[i].data_packet != NULL){
                if(sender->pointer_array[i].peer != -1){
                    send_packet(sender->pointer_array[i].peer, sender->pointer_array[i].data_packet);
                }else{
                    printf("Why peer == -1 \n");
                    return -1;
                }      
            }
        }
    }
    return 0;
}

int ctl_udp_time_out(struct bt_sender_s *sender){
    return 1;
}



