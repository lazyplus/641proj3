#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include "peer.h"
#include "send.h"

long my_get_time(){
    struct timeval cur_time;
    gettimeofday(&cur_time, NULL);
    return cur_time.tv_sec * 1000000 + cur_time.tv_usec;
}

void init_sender(bt_sender_t *sender, int id){
    sender->tail = sender->head = 0;    
    sender->window_size = START_WINDOW_SIZE;
    // for congestion control
    memset(sender->pkt_buf, 0, sizeof(sender->pkt_buf));
    sender->window_state = SLOW_START;
    sender->window_ssthresh = START_SSTHRESH_SIZE;
    sender->last_window_update_clock = my_get_time();
    sender->id = id;
    sender->last_ack_num = 0;
    sender->last_ack_cnt = 0;
    sender->is_idle = 0;
    sender->rtt = 0;
    sender->last_tick = 0;
}

int wd_lost(bt_sender_t * sender){
    int old_size = sender->window_size;
    long cur_time = my_get_time();
    switch(sender->window_state){
    case SLOW_START:
        // set window size to 1
        sender->window_size = 1;
        if (window_size_log != NULL){
            fprintf(window_size_log, "%d    %ld    %d\n",sender->id, cur_time, sender->window_size);
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
            fprintf(window_size_log, "%d    %ld    %d\n",sender->id, cur_time, sender->window_size);
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
    long cur_clock = my_get_time();
    long time_interval;
    switch(sender->window_state){
    case SLOW_START:
        // window size ++
        sender->window_size++;
        // log window change
        if (window_size_log != NULL){
            fprintf(window_size_log, "%d    %ld    %d\n",sender->id, cur_clock, sender->window_size);
            fflush(window_size_log);
        }else{
            printf("Err: empty window size log pointer\n");
        }
        if (sender->window_size > sender->window_ssthresh){
            sender->window_state = CONG_CTL;
                printf("Sender %d : %d window SLOW_START --> CONG_CTL\n", sender->peer, sender->id);
        }
        sender->last_window_update_clock = cur_clock;
        break;
    case CONG_CTL:
        time_interval = (cur_clock - sender->last_window_update_clock);
        if(time_interval >= sender->rtt){
            // window size ++
            sender->window_size++;
            // log window change
            if (window_size_log != NULL){
                fprintf(window_size_log, "%d    %ld    %d\n",sender->id, cur_clock, sender->window_size);
                fflush(window_size_log);
            }else{
                printf("Err: empty window size log pointer\n");
            }
            sender->last_window_update_clock = cur_clock;
        }
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
        sender->pkt_buf[pos].sent_ts = my_get_time();
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
    sender->pkt_buf[sender->tail].sent_ts = -1;
    sender->pkt_buf[sender->tail].resend = 0;
    if(sender->tail - sender->head < sender->window_size){
        ctl_send(sender, sender->tail);
    }
    ++ sender->tail;
    return 0;
}

int update_rtt(bt_sender_t * sender, long sent_ts){
    long cur_time = my_get_time();
    long new_rtt = cur_time - sent_ts;
    if(new_rtt * 50 < sender->rtt)
        return 0;
    sender->rtt = (BT_RTT_ALPHA * sender->rtt + (10 - BT_RTT_ALPHA) * new_rtt) / 10;
    return 0;
}

int check_too_near(bt_sender_t *sender, int ack_num){
    int i;
    for(i=sender->head; i< sender->tail; ++i){
        if(sender->pkt_buf[i].data->header.seq_num == ack_num){
            int cur_time = my_get_time();
            if(cur_time - sender->pkt_buf[i].sent_ts < sender->rtt / 50){
                return 1;
            }
        }
    }
    return 0;
}

int ctl_udp_ack(bt_sender_t *sender, int peer, data_packet_t *new_packet){
    int last_sent = sender->head + sender->window_size;

    // printf("Get ACK for %d\n", new_packet->header.ack_num);
    // duplicated ACK
    if(new_packet->header.ack_num == sender->last_ack_num){
        if( ++ sender->last_ack_cnt >= DUP_ACK_THRES){
            if(check_too_near(sender, new_packet->header.ack_num)){
                -- sender->last_ack_cnt;
            }else{
                // Not the ACK of last packet
                if(sender->last_ack_num < BUFFER_LEN){
                    wd_lost(sender);
                    ctl_send(sender, sender->last_ack_num);
                }
            }
        }
    }else if(new_packet->header.ack_num > sender->last_ack_num){
        sender->last_ack_num = new_packet->header.ack_num;
        sender->last_ack_cnt = 1;
        wd_ack(sender);
    }

    int i;
    // shift window head
    for(i = sender->head; i < sender->tail; ++i){
        if(sender->pkt_buf[i].data->header.seq_num < new_packet->header.ack_num){
            if( ++ sender->pkt_buf[i].ack == 1){
                update_rtt(sender, sender->pkt_buf[i].sent_ts);
            }
            free_packet(sender->pkt_buf[i].data);
            sender->pkt_buf[i].data = NULL;
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

    // shift window boundary and send new packets
    i = last_sent;
    if(i < sender->head)
        i = sender->head;
    for(; i<sender->tail && (i - sender->head < sender->window_size); ++i){
        if(sender->pkt_buf[i].data != NULL){
            ctl_send(sender, i);
        }
    }

    return 0;
}

int ctl_udp_time_out(bt_sender_t *sender){
    if(sender->is_idle)
        return 0;
    long cur_time = my_get_time();
    if(cur_time - sender->last_tick < sender->rtt * 2){
        return 0;
    }
    sender->last_tick = cur_time;
    int i;
    printf("tick %d %d\n", sender->head, sender->tail);
    for(i=sender->head; i<sender->tail; ++i){
        if(sender->pkt_buf[i].sent_ts == -1)
            break;
        // printf("%d %ld %ld\n", i, sender->pkt_buf[i].sent_ts, cur_time);
        if(cur_time - sender->pkt_buf[i].sent_ts > ACK_TIMEOUT){
            printf("Time Out!\n");
            if( ++ sender->pkt_buf[i].resend > RESEND_THRES){
                printf("Closed!\n");
                connection_closed(sender->peer);
                return 0;
            }else{
                wd_lost(sender);
                ctl_send(sender, i);
            }
            break;
        }
    }
    return 1;
}
