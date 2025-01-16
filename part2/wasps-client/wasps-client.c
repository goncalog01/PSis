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

#define WASPS_CONNECT 7
#define WASPS_MOVEMENT 8
#define WASPS_DISCONNECT 9
#define RESPONSE_FAIL 10
#define RESPONSE_SUCCESS 11
#define RANDOM_VALUE (1 + random() % 5)

int disconnect = 0;

void handle_signal(int signum) {
    disconnect = 1;
}


void zmq_send_WaspsConnectReq(void * requester, int n_wasps) {
    WaspsConnectReq pb_m_stuct = WASPS_CONNECT_REQ__INIT;

    pb_m_stuct.number = n_wasps;

    int size_bin_msg = wasps_connect_req__get_packed_size(&pb_m_stuct);
    char * pb_m_bin = malloc(size_bin_msg);
    wasps_connect_req__pack(&pb_m_stuct, pb_m_bin);

    int nbytes = zmq_send(requester, pb_m_bin, size_bin_msg, 0);
    free(pb_m_bin);
    assert(nbytes != -1);
}

void zmq_send_WaspsMovementReq(void * requester, int id, char dir[], int nonce) {
    WaspsMovementReq pb_m_stuct = WASPS_MOVEMENT_REQ__INIT;

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

    int size_bin_msg = wasps_movement_req__get_packed_size(&pb_m_stuct);
    char * pb_m_bin = malloc(size_bin_msg);
    wasps_movement_req__pack(&pb_m_stuct, pb_m_bin);

    int nbytes = zmq_send(requester, pb_m_bin, size_bin_msg, 0);
    free(pb_m_bin);
    assert(nbytes != -1);
}

void zmq_send_WaspsDisconnectReq(void * requester, int ids[], int nonces[], int n_wasps) {
    WaspsDisconnectReq pb_m_stuct = WASPS_DISCONNECT_REQ__INIT;

    pb_m_stuct.number = n_wasps;

    pb_m_stuct.n_ids = n_wasps;
    pb_m_stuct.ids = malloc(sizeof(int) * n_wasps);
    pb_m_stuct.n_nonces = n_wasps;
    pb_m_stuct.nonces = malloc(sizeof(int) * n_wasps);
    for (int i = 0; i < n_wasps; i++) {
        pb_m_stuct.ids[i] = ids[i];
        pb_m_stuct.nonces[i] = nonces[i];
    }

    int size_bin_msg = wasps_disconnect_req__get_packed_size(&pb_m_stuct);
    char * pb_m_bin = malloc(size_bin_msg);
    wasps_disconnect_req__pack(&pb_m_stuct, pb_m_bin);

    int nbytes = zmq_send(requester, pb_m_bin, size_bin_msg, 0);
    free(pb_m_bin);
    free(pb_m_stuct.ids);
    free(pb_m_stuct.nonces);
    assert(nbytes != -1);    
}

WaspsConnectResp * zmq_read_WaspsConnectResp(void * requester) {
    zmq_msg_t msg_raw;
    zmq_msg_init(&msg_raw);
    int n_bytes = zmq_recvmsg(requester, &msg_raw, 0);
    char *pb_msg = zmq_msg_data(&msg_raw);

    WaspsConnectResp * ret_value = wasps_connect_resp__unpack(NULL, n_bytes, pb_msg);
    zmq_msg_close(&msg_raw);
    return ret_value;
}

WaspsMovementResp * zmq_read_WaspsMovementResp(void * requester) {
    zmq_msg_t msg_raw;
    zmq_msg_init(&msg_raw);
    int n_bytes = zmq_recvmsg(requester, &msg_raw, 0);
    char *pb_msg = zmq_msg_data(&msg_raw);

    WaspsMovementResp * ret_value = wasps_movement_resp__unpack(NULL, n_bytes, pb_msg);
    zmq_msg_close(&msg_raw);
    return ret_value;
}

WaspsDisconnectResp * zmq_read_WaspsDisconnectResp(void * requester) {
    zmq_msg_t msg_raw;
    zmq_msg_init(&msg_raw);
    int n_bytes = zmq_recvmsg(requester, &msg_raw, 0);
    char *pb_msg = zmq_msg_data(&msg_raw);

    WaspsDisconnectResp * ret_value = wasps_disconnect_resp__unpack(NULL, n_bytes, pb_msg);
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
    int n_wasps = -1;

    // read optional arguments
    switch (argc) {
        case 4:
            n_wasps = atoi(argv[3]);
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

    if (n_wasps == -1) {
        n_wasps = random() % 10 + 1;
    }
    else if (n_wasps < 1 || n_wasps > 10) {
        printf("Invalid number of wasps, must be between 1 and 10.\n");
        exit(-1);
    }
    
    //connect message: message type + number of roaches to control
    char message_type[2];
    message_type[0] = WASPS_CONNECT;
    message_type[1] = '\0';

    nbytes = s_sendmore(requester, message_type);
    assert(nbytes != -1);
    zmq_send_WaspsConnectReq(requester, n_wasps);

    // response: success or fail + ids of wasps + nonce
    WaspsConnectResp * pb_m_stuct_wasps_connect = zmq_read_WaspsConnectResp(requester);
    if (pb_m_stuct_wasps_connect->response_type == RESPONSE_FAIL) {
        printf("Can't control %d wasps. Try again with a lower number.\n", n_wasps);
        exit(-1);
    }

    int *characters = pb_m_stuct_wasps_connect->characters;
    int *nonces = pb_m_stuct_wasps_connect->nonces;

    int sleep_delay;
    char dir[6];
    int wasp;
    direction_t direction;
    int n = 0;
    while (!disconnect)
    {
        n++;
        sleep_delay = random()%700000;
        usleep(sleep_delay);
        direction = random()%4;
        wasp = random()%n_wasps;
        switch (direction)
        {
        case LEFT:
            strcpy(dir, "LEFT");
            printf("%d: Wasp %d going Left\n", n, wasp + 1);
            break;
        case RIGHT:
            strcpy(dir, "RIGHT");
            printf("%d: Wasp %d going Right\n", n, wasp + 1);
           break;
        case DOWN:
            strcpy(dir, "DOWN");
            printf("%d: Wasp %d going Down\n", n, wasp + 1);
            break;
        case UP:
            strcpy(dir, "UP");
            printf("%d: Wasp %d going Up\n", n, wasp + 1);
            break;
        }

        // movement message: message type + id of wasp + direction + nonce
        char message_type[2];
        message_type[0] = WASPS_MOVEMENT;
        message_type[1] = '\0';
        nbytes = s_sendmore(requester, message_type);
        assert(nbytes != -1);
        zmq_send_WaspsMovementReq(requester, characters[wasp], dir, nonces[wasp]);

        // response: success or fail + new nonce
        WaspsMovementResp * pb_m_stuct_wasps_movement = zmq_read_WaspsMovementResp(requester);
        if (pb_m_stuct_wasps_movement->response_type == RESPONSE_FAIL) {
            continue;
        }
        nonces[wasp] = pb_m_stuct_wasps_movement->new_nonce;
    }

    //Send disconnect messages
    char disconnect_message_type[2];
    disconnect_message_type[0] = WASPS_DISCONNECT;
    disconnect_message_type[1] = '\0';

    nbytes = s_sendmore(requester, disconnect_message_type);
    assert(nbytes != -1);
    zmq_send_WaspsDisconnectReq(requester, characters, nonces, n_wasps);

    WaspsDisconnectResp * pb_m_stuct_wasps_disconnect = zmq_read_WaspsDisconnectResp(requester);
    
    free(characters);
    free(nonces);

    zmq_close (requester);
    zmq_ctx_destroy (context);

	return 0;
}