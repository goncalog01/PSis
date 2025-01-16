#include <ncurses.h>
#include "remote-char.h"
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

#define ROACHES_CONNECT 4
#define ROACHES_MOVEMENT 5
#define ROACHES_DISCONNECT 6
#define RESPONSE_FAIL 10
#define RESPONSE_SUCCESS 11
#define RANDOM_VALUE (1 + random() % 5)

int disconnect = 0;

void handle_signal(int signum) {
    disconnect = 1;
}

void zmq_send_RoachesConnectReq(void * requester, int n_roaches, int scores[]) {
    RoachesConnectReq pb_m_stuct = ROACHES_CONNECT_REQ__INIT;

    pb_m_stuct.number = n_roaches;
    pb_m_stuct.scores = malloc(sizeof(int) * n_roaches);
    for (int i = 0; i < n_roaches; i++) {
        pb_m_stuct.scores[i] = scores[i];
    }
    pb_m_stuct.n_scores = n_roaches;

    int size_bin_msg = roaches_connect_req__get_packed_size(&pb_m_stuct);
    char * pb_m_bin = malloc(size_bin_msg);
    roaches_connect_req__pack(&pb_m_stuct, pb_m_bin);

    int nbytes = zmq_send(requester, pb_m_bin, size_bin_msg, 0);
    free(pb_m_bin);
    free(pb_m_stuct.scores);
    assert(nbytes != -1);
}

void zmq_send_RoachesMovementReq(void * requester, int id, char dir[], int nonce) {
    RoachesMovementReq pb_m_stuct = ROACHES_MOVEMENT_REQ__INIT;

    pb_m_stuct.character = id;

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

    int size_bin_msg = roaches_movement_req__get_packed_size(&pb_m_stuct);
    char * pb_m_bin = malloc(size_bin_msg);
    roaches_movement_req__pack(&pb_m_stuct, pb_m_bin);

    int nbytes = zmq_send(requester, pb_m_bin, size_bin_msg, 0);
    free(pb_m_bin);
    assert(nbytes != -1);
}

void zmq_send_RoachesDisconnectReq(void * requester, int ids[], int nonces[], int n_roaches) {
    RoachesDisconnectReq pb_m_stuct = ROACHES_DISCONNECT_REQ__INIT;

    pb_m_stuct.number = n_roaches;

    pb_m_stuct.n_ids = n_roaches;
    pb_m_stuct.ids = malloc(sizeof(int) * n_roaches);
    pb_m_stuct.n_nonces = n_roaches;
    pb_m_stuct.nonces = malloc(sizeof(int) * n_roaches);
    for (int i = 0; i < n_roaches; i++) {
        pb_m_stuct.ids[i] = ids[i];
        pb_m_stuct.nonces[i] = nonces[i];
    }

    int size_bin_msg = roaches_disconnect_req__get_packed_size(&pb_m_stuct);
    char * pb_m_bin = malloc(size_bin_msg);
    roaches_disconnect_req__pack(&pb_m_stuct, pb_m_bin);

    int nbytes = zmq_send(requester, pb_m_bin, size_bin_msg, 0);
    free(pb_m_bin);
    free(pb_m_stuct.ids);
    free(pb_m_stuct.nonces);
    assert(nbytes != -1);    
}

RoachesConnectResp * zmq_read_RoachesConnectResp(void * requester) {
    zmq_msg_t msg_raw;
    zmq_msg_init(&msg_raw);
    int n_bytes = zmq_recvmsg(requester, &msg_raw, 0);
    char *pb_msg = zmq_msg_data(&msg_raw);

    RoachesConnectResp * ret_value = roaches_connect_resp__unpack(NULL, n_bytes, pb_msg);
    zmq_msg_close(&msg_raw);
    return ret_value;
}

RoachesMovementResp * zmq_read_RoachesMovementResp(void * requester) {
    zmq_msg_t msg_raw;
    zmq_msg_init(&msg_raw);
    int n_bytes = zmq_recvmsg(requester, &msg_raw, 0);
    char *pb_msg = zmq_msg_data(&msg_raw);

    RoachesMovementResp * ret_value = roaches_movement_resp__unpack(NULL, n_bytes, pb_msg);
    zmq_msg_close(&msg_raw);
    return ret_value;
}

RoachesDisconnectResp * zmq_read_RoachesDisconnectResp(void * requester) {
    zmq_msg_t msg_raw;
    zmq_msg_init(&msg_raw);
    int n_bytes = zmq_recvmsg(requester, &msg_raw, 0);
    char *pb_msg = zmq_msg_data(&msg_raw);

    RoachesDisconnectResp * ret_value = roaches_disconnect_resp__unpack(NULL, n_bytes, pb_msg);
    zmq_msg_close(&msg_raw);
    return ret_value;
}

int main(int argc, char *argv[])
{	 
    // Set up signal handler for Ctrl+C (SIGINT)
    if (signal(SIGINT, handle_signal) == SIG_ERR) {
        printf("Error setting up signal handler");
        exit(-1);
    }

    void *context = zmq_ctx_new ();
    // Connect to the server using ZMQ_REQ
    void *requester = zmq_socket (context, ZMQ_REQ);

    char host[64] = "localhost";
    char port[6] = "5556";
    int n_roaches = -1;

    // read optional arguments
    switch (argc) {
        case 4:
            n_roaches = atoi(argv[3]);
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

    int nbytes;

    if (n_roaches == -1) {
        n_roaches = random() % 10 + 1;
    }
    else if (n_roaches < 1 || n_roaches > 10) {
        printf("Invalid number of roaches, must be between 1 and 10.\n");
        exit(-1);
    }
    
    //connect message: message type + number of roaches to control + scores
    char message_type[2];
    message_type[0] = ROACHES_CONNECT;
    message_type[1] = '\0';

    nbytes = s_sendmore(requester, message_type);
    assert(nbytes != -1);

    int scores[10];
    for (int i = 0; i < n_roaches; i++) {
        scores[i] = RANDOM_VALUE;
    }

    zmq_send_RoachesConnectReq(requester, n_roaches, scores);

    // response: success or fail + ids of roaches + nonce
    RoachesConnectResp * pb_m_stuct_roaches_connect = zmq_read_RoachesConnectResp(requester);
    if (pb_m_stuct_roaches_connect->response_type == RESPONSE_FAIL) {
        printf("Can't control %d roaches. Try again with a lower number.\n", n_roaches);
        exit(-1);
    }

    int *characters = pb_m_stuct_roaches_connect->characters;
    int *nonces = pb_m_stuct_roaches_connect->nonces;

    int sleep_delay;
    char dir[6];
    int roach;
    direction_t direction;
    int n = 0;
    while (!disconnect)
    {
        n++;
        sleep_delay = random()%700000;
        usleep(sleep_delay);
        direction = random()%4;
        roach = random()%n_roaches;
        switch (direction)
        {
        case LEFT:
            strcpy(dir, "LEFT");
            printf("%d: Roach %d going Left\n", n, roach + 1);
            break;
        case RIGHT:
            strcpy(dir, "RIGHT");
            printf("%d: Roach %d going Right\n", n, roach + 1);
           break;
        case DOWN:
            strcpy(dir, "DOWN");
            printf("%d: Roach %d going Down\n", n, roach + 1);
            break;
        case UP:
            strcpy(dir, "UP");
            printf("%d: Roach %d going Up\n", n, roach + 1);
            break;
        }

        // movement message: message type + id of roach + direction + nonce
        char message_type[2];
        message_type[0] = ROACHES_MOVEMENT;
        message_type[1] = '\0';
        nbytes = s_sendmore(requester, message_type);
        assert(nbytes != -1);
        zmq_send_RoachesMovementReq(requester, characters[roach], dir, nonces[roach]);

        // response: success or fail + new nonce
        RoachesMovementResp * pb_m_stuct_roaches_movement = zmq_read_RoachesMovementResp(requester);
        if (pb_m_stuct_roaches_movement->response_type == RESPONSE_FAIL) {
            continue;
        }
        nonces[roach] = pb_m_stuct_roaches_movement->new_nonce;
    }

    //Send disconnect messages
    char disconnect_message_type[2];
    disconnect_message_type[0] = ROACHES_DISCONNECT;
    disconnect_message_type[1] = '\0';

    nbytes = s_sendmore(requester, disconnect_message_type);
    assert(nbytes != -1);
    zmq_send_RoachesDisconnectReq(requester, characters, nonces, n_roaches);

    RoachesDisconnectResp * pb_m_stuct_roaches_disconnect = zmq_read_RoachesDisconnectResp(requester);

    free(characters);
    free(nonces);

    zmq_close (requester);
    zmq_ctx_destroy (context);

	return 0;
}