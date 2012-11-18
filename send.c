#include <stdio.h>
#include <time.h>
#include "peer.h"
#include "send.h"

void init_sender(bt_sender_t *sender, int id){
    sender->tail = sender->head = 0;    
    sender->window_size = START_WINDOW_SIZE;
    // for congestion control
    sender->window_state = SLOW_START;
    sender->window_ssthresh = START_SSTHRESH_SIZE;
    sender->last_window_update_clock = clock();
    sender->id = id;
    sender->last_ack_num = 0;
    sender->last_ack_cnt = 0;
    sender->is_idle = 0;
}

int wd_lost(bt_sender_t * sender){
    int old_size = sender->window_size;
    int cur_time = clock();
    switch(sender->window_state){
    case SLOW_START:
        // set window size to 1
	sender->window_size = 1;
	if (window_size_log != NULL){
	    fprintf(window_size_log, "%d    %d    %d\n",sender->id, cur_time, sender->window_size);
        fflush(window_size_log);
	}else{
	    printf("Err: empty window size log pointer\n");
	}
        // update ssthreshold
        sender->window_ssthresh = (old_size/2 > 2)? (old_size/2):2;
	break;
    case CONG_CTL:
        // set window size to 1
	sender->window_size = 1;
	if (window_size_log != NULL){
	    fprintf(window_size_log, "%d    %d    %d\n",sender->id, cur_time, sender->window_size);
        fflush(window_size_log);
	}else{
	    printf("Err: empty window size log pointer\n");
	}
	// update ssthreshold
	sender->window_ssthresh = (old_size/2 > 2)? (old_size/2):2;
	// change to slow start
        sender->window_state = SLOW_START;
        printf("Sender %d : %d window CONG_CTL --> SLOW_START\n", sender->peer,sender->id);
        break;
    default:
        printf("Err: Illegal window state\n");
        break;
    }
    return 0;
}

int wd_ack(bt_sender_t * sender){
    int cur_clock = clock();
    double time_interval;
    switch(sender->window_state){
    case SLOW_START:
        // window size ++
        sender->window_size++;
        // log window change
	if (window_size_log != NULL){
	    fprintf(window_size_log, "%d    %d    %d\n",sender->id, cur_clock, sender->window_size);
        fflush(window_size_log);
	}else{
	    printf("Err: empty window size log pointer\n");
	}
	if (sender->window_size > sender->window_ssthresh){
	    sender->window_state = CONG_CTL;
            printf("Sender %d : %d window SLOW_START --> CONG_CTL\n", sender->peer, sender->id);
	}
        sender->last_window_update_clock = clock();
	break;
    case CONG_CTL:
        time_interval = (double)(cur_clock - sender->last_window_update_clock)/CLOCKS_PER_SEC;
        if(time_interval >= sender->rtt){
	    // window size ++
	    sender->window_size++;
	    // log window change
	    if (window_size_log != NULL){
	        fprintf(window_size_log, "%d    %d    %d\n",sender->id, cur_clock, sender->window_size);
            fflush(window_size_log);
	    }else{
	    	printf("Err: empty window size log pointer\n");
	    }
	}
	sender->last_window_update_clock = clock();
        break;
    default:
        printf("Err: Illegal window state\n");
        break;
    }
    return 0;
}

int ctl_send(bt_sender_t * sender, int pos){
    sender->pkt_buf[pos].ack = 0;
    if(pos - sender->head <= sender->window_size){
        sender->pkt_buf[pos].sent_ts = clock();
        send_packet(sender->peer, sender->pkt_buf[pos].data);
        return 0;
    }else{
        sender->pkt_buf[pos].sent_ts = -1;
        return -1;
    }
}

int ctl_udp_send(bt_sender_t *sender, int peer, data_packet_t *new_packet){
    if(new_packet == NULL){
        printf("Error: Empty data pointer in upd send\n");
        return -1;
    }

    sender->pkt_buf[sender->tail].data = new_packet;
    sender->pkt_buf[sender->tail].ack = 0;
    if(sender->tail - sender->head < sender->window_size){
        ctl_send(sender, sender->tail);
    }
    ++ sender->tail;
    return 0;
}

int update_rtt(bt_sender_t * sender, int sent_ts){
    int cur_time = clock();
    double new_rtt = (double)(cur_time - sent_ts) / CLOCKS_PER_SEC;
    sender->rtt = BT_RTT_ALPHA * sender->rtt + (1 - BT_RTT_ALPHA) * new_rtt;
    return 0;
}

int ctl_udp_ack(bt_sender_t *sender, int peer, data_packet_t *new_packet){
    int last_sent = sender->head + sender->window_size;

    // printf("Get ACK for %d\n", new_packet->header.ack_num);
    // duplicated ACK
    if(new_packet->header.ack_num == sender->last_ack_num){
        if( ++ sender->last_ack_cnt >= DUP_ACK_THRES){
            // Not the ACK of last packet
            if(sender->last_ack_num < BUFFER_LEN){
                wd_lost(sender);
                ctl_send(sender, sender->last_ack_num);
            }
        }
    }else if(new_packet->header.ack_num > sender->last_ack_num){
        sender->last_ack_num = new_packet->header.ack_num;
        sender->last_ack_cnt = 1;
        wd_ack(sender);
    }

    int i, j;
    int *head = &(sender->head);
    int *tail = &(sender->tail);
    int *window_size = &(sender->window_size);
    int data_cnt;
    int tmp = (*tail + 4*1024 - *head) % (4*1024);

    data_cnt = (tmp >(*window_size))?*window_size:tmp;

    // mark the ACK for the packet
    for(i = sender->head, j=0; j < data_cnt; ++i, ++j){
        if(sender->pkt_buf[i].data->header.seq_num < new_packet->header.ack_num){
            if( ++ sender->pkt_buf[i].ack == 1){
                update_rtt(sender, sender->pkt_buf[i].sent_ts);
            }
            free_packet(sender->pkt_buf[i].data);
        }else{
            break;
        }
    }
    sender->head = i;

    // ACK of the last packet
    if(new_packet->header.ack_num >= BUFFER_LEN){
        printf("Last Packet ACK\n");
        return connection_closed(sender->peer);
    }

    // printf("%d %d %d\n", sender->head, sender->tail, sender->window_size);
    // slide window if offset > 0
    for(i=last_sent; i<BUFFER_LEN && (i - sender->head < sender->window_size); ++i){
        // printf("Expanding %d\n", i);
        if(sender->pkt_buf[i].data != NULL){
            ctl_send(sender, i);
        }else{
            printf("What? NULL pkt?\n");
            return -1;
        }
    }

    return 0;
}

int ctl_udp_time_out(bt_sender_t *sender){
    int cur_time = clock();
    int i;
    for(i=sender->head; i<sender->tail; ++i){
        if(cur_time - sender->pkt_buf[i].sent_ts > ACK_TIMEOUT){
            wd_lost(sender);
            ctl_send(sender, i);
            break;
        }
    }
    return 1;
}
