#include <ncurses.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>  
#include <stdlib.h>
#include <zmq.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include "zhelpers.h"
#include "../lizardsNroachesNwasps.pb-c.h"
#include <ctype.h>

#define WINDOW_SIZE 30
#define LIZARD_CONNECT 1
#define LIZARD_MOVEMENT 2
#define LIZARD_DISCONNECT 3
#define RESPONSE_FAIL 10
#define RESPONSE_SUCCESS 11

int disconnect = 0;

void handle_signal(int signum) {
    disconnect = 1;
}

void zmq_send_LizardMovementReq(void * requester, char ch, char dir[], int nonce) {
    LizardMovementReq pb_m_stuct = LIZARD_MOVEMENT_REQ__INIT;

    pb_m_stuct.character.data = malloc(sizeof(ch));
    memcpy(pb_m_stuct.character.data, &ch, sizeof(ch));
    pb_m_stuct.character.len = sizeof(ch);

    if (!strcmp(dir, "LEFT")) {
        pb_m_stuct.direction = DIRECTION__LEFT;
    }
    if (!strcmp(dir, "RIGHT")) {
        pb_m_stuct.direction = DIRECTION__RIGHT;
    }
    if (!strcmp(dir, "UP")) {
        pb_m_stuct.direction = DIRECTION__UP;
    }
    if (!strcmp(dir, "DOWN")) {
        pb_m_stuct.direction = DIRECTION__DOWN;
    }
    pb_m_stuct.nonce = nonce;

    int size_bin_msg = lizard_movement_req__get_packed_size(&pb_m_stuct);
    char * pb_m_bin = malloc(size_bin_msg);
    lizard_movement_req__pack(&pb_m_stuct, pb_m_bin);

    int nbytes = zmq_send(requester, pb_m_bin, size_bin_msg, 0);
    free(pb_m_bin);
    free(pb_m_stuct.character.data);
    assert(nbytes != -1);
}

void zmq_send_LizardDisconnectReq(void * requester, char ch, int nonce) {
    LizardDisconnectReq pb_m_stuct = LIZARD_DISCONNECT_REQ__INIT;

    pb_m_stuct.character.data = malloc(sizeof(ch));
    memcpy(pb_m_stuct.character.data, &ch, sizeof(ch));
    pb_m_stuct.character.len = sizeof(ch);
    pb_m_stuct.nonce = nonce;

    int size_bin_msg = lizard_disconnect_req__get_packed_size(&pb_m_stuct);
    char * pb_m_bin = malloc(size_bin_msg);
    lizard_disconnect_req__pack(&pb_m_stuct, pb_m_bin);

    int nbytes = zmq_send(requester, pb_m_bin, size_bin_msg, 0);
    free(pb_m_bin);
    free(pb_m_stuct.character.data);
    assert(nbytes != -1);
}

LizardConnectResp * zmq_read_LizardConnectResp(void * requester) {
    zmq_msg_t msg_raw;
    zmq_msg_init(&msg_raw);
    int n_bytes = zmq_recvmsg(requester, &msg_raw, 0);
    char *pb_msg = zmq_msg_data(&msg_raw);

    LizardConnectResp * ret_value = lizard_connect_resp__unpack(NULL, n_bytes, pb_msg);
    zmq_msg_close(&msg_raw);
    return ret_value;
}

LizardMovementResp * zmq_read_LizardMovementResp(void * requester) {
    zmq_msg_t msg_raw;
    zmq_msg_init(&msg_raw);
    int n_bytes = zmq_recvmsg(requester, &msg_raw, 0);
    char *pb_msg = zmq_msg_data(&msg_raw);

    LizardMovementResp * ret_value = lizard_movement_resp__unpack(NULL, n_bytes, pb_msg);
    zmq_msg_close(&msg_raw);
    return ret_value;
}

LizardDisconnectResp * zmq_read_LizardDisconnectResp(void * requester) {
    zmq_msg_t msg_raw;
    zmq_msg_init(&msg_raw);
    int n_bytes = zmq_recvmsg(requester, &msg_raw, 0);
    char *pb_msg = zmq_msg_data(&msg_raw);

    LizardDisconnectResp * ret_value = lizard_disconnect_resp__unpack(NULL, n_bytes, pb_msg);
    zmq_msg_close(&msg_raw);
    return ret_value;
}

FieldUpdate * zmq_read_FieldUpdate(void * subscriber) {
    zmq_msg_t msg_raw;
    zmq_msg_init(&msg_raw);
    int n_bytes = zmq_recvmsg(subscriber, &msg_raw, 0);
    char *pb_msg = zmq_msg_data(&msg_raw);

    FieldUpdate * ret_value = field_update__unpack(NULL, n_bytes, pb_msg);
    zmq_msg_close(&msg_raw);
    return ret_value;
}

void draw_box(int start_row, int start_col, int height, int width) {
    for (int i = 0; i < width; ++i) {
        mvaddch(start_row, start_col + i,  ACS_HLINE);
        mvaddch(start_row + height - 1, start_col + i, ACS_HLINE);
    }
    for (int i = 0; i < height; ++i) {
        mvaddch(start_row + i, start_col, ACS_VLINE);
        mvaddch(start_row + i, start_col + width - 1, ACS_VLINE);
    }
    refresh();
}

int main(int argc, char *argv[])
{
    /*// Set up signal handler for Ctrl+C (SIGINT)
    if (signal(SIGINT, handle_signal) == SIG_ERR) {
        printf("Error setting up signal handler");
        exit(-1);
    }*/

    void *context = zmq_ctx_new ();
    // Connect to the server using ZMQ_REQ
    void *requester = zmq_socket (context, ZMQ_REQ);

    // Connect to the server using ZMQ_SUB
    void *subscriber = zmq_socket(context, ZMQ_SUB);
    zmq_connect(subscriber, "tcp://localhost:5557");
    // subscribe to topics
    zmq_setsockopt (subscriber, ZMQ_SUBSCRIBE, "field_update", 13);

    char host[64] = "localhost";
    char port[6] = "5555";

    // read optional arguments
    switch (argc) {
        case 3:
            strcpy(port, argv[2]);
        case 2:
            strcpy(host, argv[1]);
        default:
            break;
    }

    char server_addr[80];
    sprintf(server_addr, "tcp://%s:%s", host, port);

    zmq_connect(requester, server_addr);
    
    int nonce;
    char ch;

    // connect message: message type
    int nbytes;
    char message_type[2];
    message_type[0] = LIZARD_CONNECT;
    message_type[1] = '\0';

    nbytes = s_send(requester, message_type);
    assert(nbytes != -1);

    // response: success or fail + char of lizard + nonce
    LizardConnectResp * pb_m_stuct_lizard_connect = zmq_read_LizardConnectResp(requester);
    if (pb_m_stuct_lizard_connect->response_type == RESPONSE_FAIL) {
        exit(-1);
    }
    ch = pb_m_stuct_lizard_connect->character.data[0];
    nonce = pb_m_stuct_lizard_connect->nonce;

    int n = 0;
    char dir[6];

    int scores[26];
    for (int i = 0; i < 26; ++i) {
        scores[i] = INT_MIN;
    }
    scores[ch-'a'] = 0;
    
    char *subscription;

    initscr();		    	
	cbreak();				
    keypad(stdscr, TRUE);   
	noecho();
    nodelay(stdscr, TRUE);

    /* creates a window and draws a border */
    //WINDOW * my_win = newwin(WINDOW_SIZE, WINDOW_SIZE, 0, 0);
    //box(my_win, 0 , 0);
    draw_box(0, 0, WINDOW_SIZE, WINDOW_SIZE);
	//wrefresh(my_win);
    refresh();
    
    move(WINDOW_SIZE,0);
    clrtoeol();
    mvprintw(WINDOW_SIZE,0,"Your lizard is: %c", ch);
    move(WINDOW_SIZE+1,0);
    clrtoeol();
    mvprintw(WINDOW_SIZE+1,0,"Your score is: %d", scores[ch-'a']);
    move(WINDOW_SIZE+2,0);
    clrtoeol();
    mvprintw(WINDOW_SIZE+2,0,"Press Q to disconnect");
    refresh();

    int key;
    do {
    	key = getch();
        n++;
        switch (key)
        {
        case KEY_LEFT:
            strcpy(dir, "LEFT");
            break;
        case KEY_RIGHT:
            strcpy(dir, "RIGHT");
            break;
        case KEY_DOWN:
            strcpy(dir, "DOWN");
            break;
        case KEY_UP:
            strcpy(dir, "UP");
            break;
        case 'q':
        case 'Q':
            key = 'q';
            disconnect = 1;
            break;
        default:
            key = 'x';
            break;
        }

        // send the movement message
        if (key != 'x' && key != 'q') {

            // movement message: message type + char of lizard + direction + nonce
            message_type[0] = LIZARD_MOVEMENT;
            nbytes = s_sendmore(requester, message_type);
            assert (nbytes != -1);
            zmq_send_LizardMovementReq(requester, ch, dir, nonce);

            // response: success or fail + score + new nonce
            LizardMovementResp * pb_m_stuct_lizard_movement = zmq_read_LizardMovementResp(requester);
            if (pb_m_stuct_lizard_movement->response_type == RESPONSE_FAIL) {
                continue;
            }
            nonce = pb_m_stuct_lizard_movement->new_nonce;

        }

        if (key != 'q' && !disconnect) {
            //FIELD UPDATE
            move(WINDOW_SIZE+1,0);
            clrtoeol();
            mvprintw(WINDOW_SIZE+1,0,"Your score is: %d", scores[ch-'a']);
            // receive char and position
            subscription = s_recv(subscriber);
            assert(subscription != NULL);
            FieldUpdate * pb_m_stuct_field_update = zmq_read_FieldUpdate(subscriber);

            //draw mark on new position
            char new_ch = pb_m_stuct_field_update->character.data[0];
            /*wmove(my_win, pb_m_stuct_field_update->x, pb_m_stuct_field_update->y);
            waddch(my_win, ch| A_BOLD);
            wrefresh(my_win);*/
            move(pb_m_stuct_field_update->x, pb_m_stuct_field_update->y);
            addch(new_ch| A_BOLD);
            refresh();

            if (pb_m_stuct_field_update->has_id) {
                scores[pb_m_stuct_field_update->id.data[0]-'a'] = pb_m_stuct_field_update->new_score;
            }

            //draw scores
            for (int i = 0; i < 26; i++) {
                move(i, WINDOW_SIZE+2);
                clrtoeol();
            }
            int offset = 0;
            for (int i = 0; i < 26; i++) {
                if (scores[i] != INT_MIN && ch != i+'a') {
                    mvprintw(offset, WINDOW_SIZE+2, "Player %c score: %d", 'a'+i, scores[i]);
                    offset++;
                }
            }
            refresh();
        }

    } while (key != 'q' && key != 'Q' && !disconnect);

    // send disconnect message to server
    // disconnect message: message type + char of lizard + nonce
    message_type[0] = LIZARD_DISCONNECT;
    nbytes = s_sendmore(requester, message_type);
    zmq_send_LizardDisconnectReq(requester, ch, nonce);

    // response: success or fail
    LizardDisconnectResp * pb_m_stuct_lizard_disconnect = zmq_read_LizardDisconnectResp(requester);

    free(subscription);
    zmq_close(subscriber);
    zmq_close(requester);
    zmq_ctx_destroy (context);

    endwin();			/* End curses mode */

	return 0;
}