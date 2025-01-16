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
#include <ctype.h>

#define ROACHES_CONNECT 3
#define ROACHES_MOVEMENT 4
#define RESPONSE_SUCCESS 7
#define RESPONSE_FAIL 8
#define RANDOM_VALUE (1 + random() % 5)

int send_integer(void *socket, int integer, int flag) {
    int to_send[1];
    to_send[0] = integer;
    int nbytes = zmq_send(socket, to_send, 1*sizeof(int), flag);
    return nbytes;
}

int receive_integer(void *socket) {
    int integer[1];
    int nbytes = zmq_recv(socket, integer, 1*sizeof(int), 0);
    if (nbytes == -1) {
        return -1;
    }
    return integer[0];
}

int main(int argc, char *argv[])
{	 

    void *context = zmq_ctx_new ();
    // Connect to the server using ZMQ_REQ
    void *requester = zmq_socket (context, ZMQ_REQ);

    char host[64] = "localhost";
    char port[6] = "5555";
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

    int characters[11];
    int nonces[10];
    int nbytes;

    if (n_roaches == -1) {
        n_roaches = random() % 10 + 1;
    }
    else if (n_roaches < 1 || n_roaches > 10) {
        printf("Invalid number of roaches, must be between 1 and 10.\n");
        exit(-1);
    }
    
    //connect message: message type + number of roaches to control + scoress
    char message_type[2];
    message_type[0] = ROACHES_CONNECT;
    message_type[1] = '\0';

    nbytes = s_sendmore(requester, message_type);
    assert(nbytes != -1);

    char roaches_number[2];
    roaches_number[0] = n_roaches;
    roaches_number[1] = '\0';
    nbytes = s_sendmore(requester, roaches_number);
    assert(nbytes != -1);

    char score[2];
    score[1] = '\0';
    for (int i = 0; i < n_roaches - 1; i++) {
        score[0] = RANDOM_VALUE;
        nbytes = s_sendmore(requester, score);
        assert(nbytes != -1);
    }
    score[0] = RANDOM_VALUE;
    nbytes = s_send(requester, score);
    assert(nbytes != -1);

    // response: success or fail + ids of roaches + nonce
    char *response = s_recv(requester);
    assert(response != NULL);
    if (response[0] == RESPONSE_SUCCESS) {
        free(response);
        nbytes = zmq_recv(requester, characters, 11*sizeof(int), 0);
        assert(nbytes != -1);
        nbytes = zmq_recv(requester, nonces, 10*sizeof(int), 0);
        assert(nbytes != -1);
    }
    else {
        printf("Can't control %d roaches. Try again with a lower number.\n", n_roaches);
        free(response);
        exit(-1);
    }

    int sleep_delay;
    char dir[6];
    int roach;
    direction_t direction;
    int n = 0;
    while (1)
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
        nbytes = send_integer(requester, characters[roach], ZMQ_SNDMORE);
        assert(nbytes != -1);
        nbytes = s_sendmore(requester, dir);
        assert(nbytes != -1);
        nbytes = send_integer(requester, nonces[roach], 0);
        assert(nbytes != -1);

        // response: success or fail + new nonce
        char *movement_response = s_recv(requester);
        assert (movement_response != NULL);

        int new_nonce = receive_integer(requester);
        assert(new_nonce != -1);
        nonces[roach] = new_nonce;

        free(movement_response);
    }

    free(characters);

    zmq_close (requester);
    zmq_ctx_destroy (context);

	return 0;
}