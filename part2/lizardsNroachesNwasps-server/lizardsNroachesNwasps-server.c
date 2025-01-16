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
#include <time.h>
#include <pthread.h>


#define WINDOW_SIZE 30
#define MAX_LIZARDS 26
#define MAX_ROACHES_WASPS ((WINDOW_SIZE - 2)*(WINDOW_SIZE - 2))/3
#define RANDOM_POS (1 + random() % (WINDOW_SIZE - 2))
#define min(a, b) ((a) < (b) ? (a) : (b))

#define LIZARD_CONNECT 1
#define LIZARD_MOVEMENT 2
#define LIZARD_DISCONNECT 3
#define ROACHES_CONNECT 4
#define ROACHES_MOVEMENT 5
#define ROACHES_DISCONNECT 6
#define WASPS_CONNECT 7
#define WASPS_MOVEMENT 8
#define WASPS_DISCONNECT 9
#define RESPONSE_FAIL 10
#define RESPONSE_SUCCESS 11

typedef struct ch_info_t {
    int ch;
    int pos_x, pos_y;
    int score;
    time_t timestamp;
    int tail_size;
    int body_x[5];
    int body_y[5];
    int nonce;
} ch_info_t;

direction_t random_direction() {
    return  random()%4;
}

ch_info_t lizards_data[MAX_LIZARDS];
ch_info_t roaches_data[MAX_ROACHES_WASPS];
ch_info_t wasps_data[MAX_ROACHES_WASPS];
int n_lizards = 0;
int n_roaches = 0;
int n_wasps = 0;

void *context;

pthread_mutex_t lizards_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t roaches_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t wasps_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t draw_mutex = PTHREAD_MUTEX_INITIALIZER;

int new_position(int *x, int *y, direction_t direction, ch_info_t lizards_data[], int n_lizards, ch_info_t roaches_data[], int n_roaches, ch_info_t wasps_data[], int n_wasps) {
    int new_x = *x;
    int new_y = *y;
    switch (direction) {
        case UP:
            (new_x) --;
            if(new_x ==0)
                new_x = 2;
            break;
        case DOWN:
            (new_x) ++;
            if(new_x ==WINDOW_SIZE-1)
                new_x = WINDOW_SIZE-3;
            break;
        case LEFT:
            (new_y) --;
            if(new_y ==0)
                new_y = 2;
            break;
        case RIGHT:
            (new_y) ++;
            if(new_y ==WINDOW_SIZE-1)
                new_y = WINDOW_SIZE-3;
            break;
        default:
            break;
    }
    // check for collision with lizard head
    for (int i = 0; i < n_lizards; i++) {
        if (lizards_data[i].ch != -1 && lizards_data[i].pos_x == new_x && lizards_data[i].pos_y == new_y) {
            return lizards_data[i].ch;
        }
    }
    // check for collision with roach
    for (int i = 0; i < n_roaches; i++) {
        if (roaches_data[i].ch != -1 && roaches_data[i].pos_x == new_x && roaches_data[i].pos_y == new_y) {
            *x = new_x;
            *y = new_y;
            return roaches_data[i].ch;
        }
    }
    // check for collision with wasp
    for (int i = 0; i < n_wasps; i++) {
        if (wasps_data[i].ch != -1 && wasps_data[i].pos_x == new_x && wasps_data[i].pos_y == new_y) {
            return wasps_data[i].ch;
        }
    }
    *x = new_x;
    *y = new_y;
    return 0;
}

int find_ch_info(ch_info_t data[], int n_data, int ch) {

    for (int i = 0 ; i < n_data; i++){
        if(ch == data[i].ch){
            return i;
        }
    }
    return -1;
}

void random_empty_position(ch_info_t lizards_data[], int n_lizards, ch_info_t roaches_data[], int n_roaches, ch_info_t wasps_data[], int n_wasps, int *pos_x, int *pos_y) {
    int x, y;
    int occupied = true;
    while (occupied) {
        occupied = false;
        x = RANDOM_POS;
        y = RANDOM_POS;
        for (int i = 0; i < n_lizards; i++) {
            if (lizards_data[i].pos_x == x && lizards_data[i].pos_y == y)
                occupied = true;
                break;
        }
        for (int i = 0; i < n_roaches; i++) {
            if (roaches_data[i].pos_x == x && roaches_data[i].pos_y == y)
                occupied = true;
                break;
        }
        for (int i = 0; i < n_wasps; i++) {
            if (wasps_data[i].pos_x == x && wasps_data[i].pos_y == y)
                occupied = true;
                break;
        }
    }
    *pos_x = x;
    *pos_y = y;
}

int random_unused_character(ch_info_t data[], int n_data, int starting_char, int range) {
    int ch;
    int occupied = true;
    while (occupied) {
        occupied = false;
        ch = starting_char + randof(range);
        for (int i = 0; i < n_data; i++) {
            if (data[i].ch == ch)
                occupied = true;
                break;
        }
    }
    return ch;
}

int no_ch_in_pos(ch_info_t data[], int n_data, int x, int y) {
    for (int i = 0; i < n_data; i++) {
        if (data[i].pos_x == x && data[i].pos_y == y && data[i].timestamp == 0) {
            return false;
        }
    }
    return true;
}

// Function to generate a random nonce
int generate_nonce(int i) {
    // Seed the random number generator with the current time
    srand((unsigned int)time(NULL) + i);

    // Generate a random number as the nonce
    int nonce = rand();

    return nonce;
}

void zmq_send_LizardConnectResp(void * responder, int response_type, char ch, int nonce) {
    LizardConnectResp pb_m_stuct = LIZARD_CONNECT_RESP__INIT;
    if (response_type == RESPONSE_SUCCESS) {
        pb_m_stuct.response_type = RESPONSE_TYPE__RESPONSE_SUCCESS;
    }
    else {
        pb_m_stuct.response_type == RESPONSE_TYPE__RESPONSE_FAIL;
    }
    pb_m_stuct.character.data = malloc(sizeof(ch));
    memcpy(pb_m_stuct.character.data, &ch, sizeof(ch));
    pb_m_stuct.character.len = sizeof(ch);
    pb_m_stuct.nonce = nonce;

    int size_bin_msg = lizard_connect_resp__get_packed_size(&pb_m_stuct);
    char * pb_m_bin = malloc(size_bin_msg);
    lizard_connect_resp__pack(&pb_m_stuct, pb_m_bin);

    int nbytes = zmq_send(responder, pb_m_bin, size_bin_msg, 0);
    free(pb_m_bin);
    free(pb_m_stuct.character.data);
    assert(nbytes != -1);
}

void zmq_send_LizardMovementResp(void * responder, int response_type, int score, int nonce) {
    LizardMovementResp pb_m_stuct = LIZARD_MOVEMENT_RESP__INIT;
    if (response_type == RESPONSE_SUCCESS) {
        pb_m_stuct.response_type = RESPONSE_TYPE__RESPONSE_SUCCESS;
    }
    else {
        pb_m_stuct.response_type == RESPONSE_TYPE__RESPONSE_FAIL;
    }
    pb_m_stuct.score = score;
    pb_m_stuct.new_nonce = nonce;

    int size_bin_msg = lizard_movement_resp__get_packed_size(&pb_m_stuct);
    char * pb_m_bin = malloc(size_bin_msg);
    lizard_movement_resp__pack(&pb_m_stuct, pb_m_bin);

    int nbytes = zmq_send(responder, pb_m_bin, size_bin_msg, 0);
    free(pb_m_bin);
    assert(nbytes != -1);
}

void zmq_send_LizardDisconnectResp(void * responder, int response_type) {
    LizardDisconnectResp pb_m_stuct = LIZARD_DISCONNECT_RESP__INIT;
    if (response_type == RESPONSE_SUCCESS) {
        pb_m_stuct.response_type = RESPONSE_TYPE__RESPONSE_SUCCESS;
    }
    else {
        pb_m_stuct.response_type == RESPONSE_TYPE__RESPONSE_FAIL;
    }

    int size_bin_msg = lizard_disconnect_resp__get_packed_size(&pb_m_stuct);
    char * pb_m_bin = malloc(size_bin_msg);
    lizard_disconnect_resp__pack(&pb_m_stuct, pb_m_bin);

    int nbytes = zmq_send(responder, pb_m_bin, size_bin_msg, 0);
    free(pb_m_bin);
    assert(nbytes != -1);
}

void zmq_send_RoachesConnectResp(void * responder, int response_type, int n_roaches, int characters[], int nonces[]) {
    RoachesConnectResp pb_m_stuct = ROACHES_CONNECT_RESP__INIT;
    if (response_type == RESPONSE_SUCCESS) {
        pb_m_stuct.response_type = RESPONSE_TYPE__RESPONSE_SUCCESS;
    }
    else {
        pb_m_stuct.response_type == RESPONSE_TYPE__RESPONSE_FAIL;
    }
    pb_m_stuct.n_characters = n_roaches;
    pb_m_stuct.characters = malloc(sizeof(int) * n_roaches);
    pb_m_stuct.n_nonces = n_roaches;
    pb_m_stuct.nonces = malloc(sizeof(int) * n_roaches);
    for (int i = 0; i < n_roaches; i++) {
        pb_m_stuct.characters[i] = characters[i];
        pb_m_stuct.nonces[i] = nonces[i];
    }

    int size_bin_msg = roaches_connect_resp__get_packed_size(&pb_m_stuct);
    char * pb_m_bin = malloc(size_bin_msg);
    roaches_connect_resp__pack(&pb_m_stuct, pb_m_bin);

    int nbytes = zmq_send(responder, pb_m_bin, size_bin_msg, 0);
    free(pb_m_bin);
    free(pb_m_stuct.characters);
    free(pb_m_stuct.nonces);
    assert(nbytes != -1);
}

void zmq_send_RoachesMovementResp(void * responder, int response_type, int nonce) {
    RoachesMovementResp pb_m_stuct = ROACHES_MOVEMENT_RESP__INIT;
    if (response_type == RESPONSE_SUCCESS) {
        pb_m_stuct.response_type = RESPONSE_TYPE__RESPONSE_SUCCESS;
    }
    else {
        pb_m_stuct.response_type == RESPONSE_TYPE__RESPONSE_FAIL;
    }
    pb_m_stuct.new_nonce = nonce;

    int size_bin_msg = roaches_movement_resp__get_packed_size(&pb_m_stuct);
    char * pb_m_bin = malloc(size_bin_msg);
    roaches_movement_resp__pack(&pb_m_stuct, pb_m_bin);

    int nbytes = zmq_send(responder, pb_m_bin, size_bin_msg, 0);
    free(pb_m_bin);
    assert(nbytes != -1);
}

void zmq_send_RoachesDisconnectResp(void * responder, int response_type) {
    RoachesDisconnectResp pb_m_stuct = ROACHES_DISCONNECT_RESP__INIT;
    if (response_type == RESPONSE_SUCCESS) {
        pb_m_stuct.response_type = RESPONSE_TYPE__RESPONSE_SUCCESS;
    }
    else {
        pb_m_stuct.response_type == RESPONSE_TYPE__RESPONSE_FAIL;
    }

    int size_bin_msg = roaches_disconnect_resp__get_packed_size(&pb_m_stuct);
    char * pb_m_bin = malloc(size_bin_msg);
    roaches_disconnect_resp__pack(&pb_m_stuct, pb_m_bin);

    int nbytes = zmq_send(responder, pb_m_bin, size_bin_msg, 0);
    free(pb_m_bin);
    assert(nbytes != -1);
}

void zmq_send_WaspsConnectResp(void * responder, int response_type, int n_wasps, int characters[], int nonces[]) {
    WaspsConnectResp pb_m_stuct = WASPS_CONNECT_RESP__INIT;
    if (response_type == RESPONSE_SUCCESS) {
        pb_m_stuct.response_type = RESPONSE_TYPE__RESPONSE_SUCCESS;
    }
    else {
        pb_m_stuct.response_type == RESPONSE_TYPE__RESPONSE_FAIL;
    }
    pb_m_stuct.n_characters = n_wasps;
    pb_m_stuct.characters = malloc(sizeof(int) * n_wasps);
    pb_m_stuct.n_nonces = n_wasps;
    pb_m_stuct.nonces = malloc(sizeof(int) * n_wasps);
    for (int i = 0; i < n_wasps; i++) {
        pb_m_stuct.characters[i] = characters[i];
        pb_m_stuct.nonces[i] = nonces[i];
    }

    int size_bin_msg = wasps_connect_resp__get_packed_size(&pb_m_stuct);
    char * pb_m_bin = malloc(size_bin_msg);
    wasps_connect_resp__pack(&pb_m_stuct, pb_m_bin);

    int nbytes = zmq_send(responder, pb_m_bin, size_bin_msg, 0);
    free(pb_m_bin);
    free(pb_m_stuct.characters);
    free(pb_m_stuct.nonces);
    assert(nbytes != -1);
}

void zmq_send_WaspsMovementResp(void * responder, int response_type, int nonce) {
    WaspsMovementResp pb_m_stuct = WASPS_MOVEMENT_RESP__INIT;
    if (response_type == RESPONSE_SUCCESS) {
        pb_m_stuct.response_type = RESPONSE_TYPE__RESPONSE_SUCCESS;
    }
    else {
        pb_m_stuct.response_type == RESPONSE_TYPE__RESPONSE_FAIL;
    }
    pb_m_stuct.new_nonce = nonce;

    int size_bin_msg = wasps_movement_resp__get_packed_size(&pb_m_stuct);
    char * pb_m_bin = malloc(size_bin_msg);
    wasps_movement_resp__pack(&pb_m_stuct, pb_m_bin);

    int nbytes = zmq_send(responder, pb_m_bin, size_bin_msg, 0);
    free(pb_m_bin);
    assert(nbytes != -1);
}

void zmq_send_WaspsDisconnectResp(void * responder, int response_type) {
    WaspsDisconnectResp pb_m_stuct = WASPS_DISCONNECT_RESP__INIT;
    if (response_type == RESPONSE_SUCCESS) {
        pb_m_stuct.response_type = RESPONSE_TYPE__RESPONSE_SUCCESS;
    }
    else {
        pb_m_stuct.response_type == RESPONSE_TYPE__RESPONSE_FAIL;
    }

    int size_bin_msg = wasps_disconnect_resp__get_packed_size(&pb_m_stuct);
    char * pb_m_bin = malloc(size_bin_msg);
    wasps_disconnect_resp__pack(&pb_m_stuct, pb_m_bin);

    int nbytes = zmq_send(responder, pb_m_bin, size_bin_msg, 0);
    free(pb_m_bin);
    assert(nbytes != -1);
}

void zmq_send_FieldUpdate(void * publisher, int x, int y, char ch, int id, int score) {
    zmq_send(publisher, "field_update", 13 * sizeof(char), ZMQ_SNDMORE);

    FieldUpdate pb_m_stuct = FIELD_UPDATE__INIT;
    pb_m_stuct.x = x;
    pb_m_stuct.y = y;
    pb_m_stuct.character.data = malloc(sizeof(ch));
    memcpy(pb_m_stuct.character.data, &ch, sizeof(ch));
    pb_m_stuct.character.len = sizeof(ch);
    if (id != -1) {
        pb_m_stuct.has_id = 1;
        pb_m_stuct.id.data = malloc(sizeof(id));
        memcpy(pb_m_stuct.id.data, &id, sizeof(id));
        pb_m_stuct.id.len = sizeof(id);
        pb_m_stuct.has_new_score = 1;
        pb_m_stuct.new_score = score;
    }
    else {
        pb_m_stuct.has_id = 0;
        pb_m_stuct.has_new_score = 0;
    }

    int size_bin_msg = field_update__get_packed_size(&pb_m_stuct);
    char * pb_m_bin = malloc(size_bin_msg);
    field_update__pack(&pb_m_stuct, pb_m_bin);

    int nbytes = zmq_send(publisher, pb_m_bin, size_bin_msg, 0);
    free(pb_m_bin);
    free(pb_m_stuct.character.data);
    if (id != -1) {
        free(pb_m_stuct.id.data);
    }
    assert(nbytes != -1);
}

LizardMovementReq * zmq_read_LizardMovementReq(void * responder) {
    zmq_msg_t msg_raw;
    zmq_msg_init(&msg_raw);
    int n_bytes = zmq_recvmsg(responder, &msg_raw, 0);
    char *pb_msg = zmq_msg_data(&msg_raw);

    LizardMovementReq * ret_value = lizard_movement_req__unpack(NULL, n_bytes, pb_msg);
    zmq_msg_close(&msg_raw);
    return ret_value;
}

LizardDisconnectReq * zmq_read_LizardDisconnectReq(void * responder) {
    zmq_msg_t msg_raw;
    zmq_msg_init(&msg_raw);
    int n_bytes = zmq_recvmsg(responder, &msg_raw, 0);
    char *pb_msg = zmq_msg_data(&msg_raw);

    LizardDisconnectReq * ret_value = lizard_disconnect_req__unpack(NULL, n_bytes, pb_msg);
    zmq_msg_close(&msg_raw);
    return ret_value;
}

RoachesConnectReq * zmq_read_RoachesConnectReq(void * responder) {
    zmq_msg_t msg_raw;
    zmq_msg_init(&msg_raw);
    int n_bytes = zmq_recvmsg(responder, &msg_raw, 0);
    char *pb_msg = zmq_msg_data(&msg_raw);

    RoachesConnectReq * ret_value = roaches_connect_req__unpack(NULL, n_bytes, pb_msg);
    zmq_msg_close(&msg_raw);
    return ret_value;
}

RoachesMovementReq * zmq_read_RoachesMovementReq(void * responder) {
    zmq_msg_t msg_raw;
    zmq_msg_init(&msg_raw);
    int n_bytes = zmq_recvmsg(responder, &msg_raw, 0);
    char *pb_msg = zmq_msg_data(&msg_raw);

    RoachesMovementReq * ret_value = roaches_movement_req__unpack(NULL, n_bytes, pb_msg);
    zmq_msg_close(&msg_raw);
    return ret_value;
}

RoachesDisconnectReq * zmq_read_RoachesDisconnectReq(void * responder) {
    zmq_msg_t msg_raw;
    zmq_msg_init(&msg_raw);
    int n_bytes = zmq_recvmsg(responder, &msg_raw, 0);
    char *pb_msg = zmq_msg_data(&msg_raw);

    RoachesDisconnectReq * ret_value = roaches_disconnect_req__unpack(NULL, n_bytes, pb_msg);
    zmq_msg_close(&msg_raw);
    return ret_value;
}

WaspsConnectReq * zmq_read_WaspsConnectReq(void * responder) {
    zmq_msg_t msg_raw;
    zmq_msg_init(&msg_raw);
    int n_bytes = zmq_recvmsg(responder, &msg_raw, 0);
    char *pb_msg = zmq_msg_data(&msg_raw);

    WaspsConnectReq * ret_value = wasps_connect_req__unpack(NULL, n_bytes, pb_msg);
    zmq_msg_close(&msg_raw);
    return ret_value;
}

WaspsMovementReq * zmq_read_WaspsMovementReq(void * responder) {
    zmq_msg_t msg_raw;
    zmq_msg_init(&msg_raw);
    int n_bytes = zmq_recvmsg(responder, &msg_raw, 0);
    char *pb_msg = zmq_msg_data(&msg_raw);

    WaspsMovementReq * ret_value = wasps_movement_req__unpack(NULL, n_bytes, pb_msg);
    zmq_msg_close(&msg_raw);
    return ret_value;
}

WaspsDisconnectReq * zmq_read_WaspsDisconnectReq(void * responder) {
    zmq_msg_t msg_raw;
    zmq_msg_init(&msg_raw);
    int n_bytes = zmq_recvmsg(responder, &msg_raw, 0);
    char *pb_msg = zmq_msg_data(&msg_raw);

    WaspsDisconnectReq * ret_value = wasps_disconnect_req__unpack(NULL, n_bytes, pb_msg);
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

void send_board(void *socket, ch_info_t lizards_data[], int n_lizards, ch_info_t roaches_data[], int n_roaches, ch_info_t wasps_data[], int n_wasps) {
    int res = pthread_mutex_lock(&lizards_mutex);
    assert(res == 0);
    res = pthread_mutex_lock(&roaches_mutex);
    assert(res == 0);
    res = pthread_mutex_lock(&wasps_mutex);
    assert(res == 0);
    for (int i = 0; i < n_lizards; i++) {
        if (lizards_data[i].ch != -1)
            zmq_send_FieldUpdate(socket, lizards_data[i].pos_x, lizards_data[i].pos_y, lizards_data[i].ch, lizards_data[i].ch, lizards_data[i].score);
    }
    for (int i = 0; i < n_roaches; i++) {
        if (roaches_data[i].ch != -1)
            zmq_send_FieldUpdate(socket, roaches_data[i].pos_x, roaches_data[i].pos_y, roaches_data[i].score + '0', -1, -1);
    }
    for (int i = 0; i < n_wasps; i++) {
        if (wasps_data[i].ch != -1)
            zmq_send_FieldUpdate(socket, wasps_data[i].pos_x, wasps_data[i].pos_y, '#', -1, -1);
    }
    res = pthread_mutex_unlock(&wasps_mutex);
    assert(res == 0);
    res = pthread_mutex_unlock(&roaches_mutex);
    assert(res == 0);
    res = pthread_mutex_unlock(&lizards_mutex);
    assert(res == 0);
}

void * lizard_thread_function(void *args) {
    void *responder = zmq_socket (context, ZMQ_REP);
    int rc = zmq_connect(responder, "inproc://lizards_backend");
    assert (rc == 0);

    void *field_update_pusher = zmq_socket (context, ZMQ_PUSH);
    rc = zmq_connect(field_update_pusher, "inproc://field_update");
    assert (rc == 0);
    
    int ch;
    int pos_x;
    int pos_y;
    int nonce;
    direction_t  direction;

    while (1) {

        char *msg_type = s_recv(responder);
        assert(msg_type != NULL);

        switch (msg_type[0]) {
        case LIZARD_CONNECT:

            int res = pthread_mutex_lock(&lizards_mutex);
            assert(res == 0);

            // if full of lizards deny request
            if (n_lizards == MAX_LIZARDS) {
                zmq_send_LizardConnectResp(responder, RESPONSE_FAIL, 'x', -1);
                break;
            }

            //assign random char and place it at random position
            ch = random_unused_character(lizards_data, MAX_LIZARDS, 'a', MAX_LIZARDS);

            res = pthread_mutex_lock(&roaches_mutex);
            assert(res == 0);
            res = pthread_mutex_lock(&wasps_mutex);
            assert(res == 0);
            random_empty_position(lizards_data, MAX_LIZARDS, roaches_data, MAX_ROACHES_WASPS, wasps_data, MAX_ROACHES_WASPS, &pos_x, &pos_y);
            
            res = pthread_mutex_unlock(&wasps_mutex);
            assert(res == 0);
            res = pthread_mutex_unlock(&roaches_mutex);
            assert(res == 0);

            int lizard_connect_ch_pos;
            for (int i = 0; i < MAX_LIZARDS; i++) {
                if (lizards_data[i].ch == -1) {
                    lizard_connect_ch_pos = i;
                    break;
                }
            }

            nonce = generate_nonce(0);

            lizards_data[lizard_connect_ch_pos].ch = ch;
            lizards_data[lizard_connect_ch_pos].pos_x = pos_x;
            lizards_data[lizard_connect_ch_pos].pos_y = pos_y;
            lizards_data[lizard_connect_ch_pos].score = 0;
            lizards_data[lizard_connect_ch_pos].nonce = nonce;
            n_lizards++;

            zmq_send_FieldUpdate(field_update_pusher, pos_x, pos_y, ch, ch, 0);

            // figure out tail size and direction
            int dx = 0;
            int dy = 0;
            direction_t initial_direction = random()%4;
            switch (initial_direction) {
                case LEFT:
                    lizards_data[lizard_connect_ch_pos].tail_size = min(5, WINDOW_SIZE - 2 - pos_y);
                    dy = 1;
                    break;
                case RIGHT:
                    lizards_data[lizard_connect_ch_pos].tail_size = min(5, pos_y - 1);
                    dy = -1;
                    break;
                case DOWN:
                    lizards_data[lizard_connect_ch_pos].tail_size = min(5, pos_x - 1);
                    dx = -1;
                    break;
                case UP:
                    lizards_data[lizard_connect_ch_pos].tail_size = min(5, WINDOW_SIZE - 2 - pos_x);
                    dx = 1;
                    break;
            }
            //draw tail
            res = pthread_mutex_lock(&roaches_mutex);
            assert(res == 0);
            res = pthread_mutex_lock(&wasps_mutex);
            assert(res == 0);

            for (int i = 1; i < lizards_data[lizard_connect_ch_pos].tail_size + 1; i++) {
                if (no_ch_in_pos(lizards_data, MAX_LIZARDS, pos_x + dx*i, pos_y + dy*i) && no_ch_in_pos(roaches_data, MAX_ROACHES_WASPS, pos_x + dx*i, pos_y + dy*i) && no_ch_in_pos(wasps_data, MAX_ROACHES_WASPS, pos_x + dx*i, pos_y + dy*i)) {
                    lizards_data[lizard_connect_ch_pos].body_x[i-1] = pos_x + dx*i;
                    lizards_data[lizard_connect_ch_pos].body_y[i-1] = pos_y + dy*i;

                    zmq_send_FieldUpdate(field_update_pusher, pos_x + dx*i, pos_y + dy*i, '.', -1, -1);
                }
            }
            res = pthread_mutex_unlock(&wasps_mutex);
            assert(res == 0);
            res = pthread_mutex_unlock(&roaches_mutex);
            assert(res == 0);

            // response: success or fail + char of lizard + nonce
            zmq_send_LizardConnectResp(responder, RESPONSE_SUCCESS, ch, nonce);
            
            res = pthread_mutex_unlock(&lizards_mutex);
            assert(res == 0);

            send_board(field_update_pusher, lizards_data, MAX_LIZARDS, roaches_data, MAX_ROACHES_WASPS, wasps_data, MAX_ROACHES_WASPS);
            break;

        case LIZARD_MOVEMENT:
            LizardMovementReq * pb_m_stuct_lizard_movement = zmq_read_LizardMovementReq(responder);

            res = pthread_mutex_lock(&lizards_mutex);
            assert(res == 0);

            int lizard_movement_ch_pos = find_ch_info(lizards_data, MAX_LIZARDS, pb_m_stuct_lizard_movement->character.data[0]);

            // check the nonce
            if (lizards_data[lizard_movement_ch_pos].nonce != pb_m_stuct_lizard_movement->nonce) {
                zmq_send_LizardMovementResp(responder, RESPONSE_FAIL, -1, -1);
                break;
            }

            if (pb_m_stuct_lizard_movement->direction == DIRECTION__LEFT) {
                direction = LEFT;
            }
            else if (pb_m_stuct_lizard_movement->direction == DIRECTION__RIGHT) {
                direction = RIGHT;
            }
            else if (pb_m_stuct_lizard_movement->direction == DIRECTION__UP) {
                direction = UP;
            }
            else if (pb_m_stuct_lizard_movement->direction == DIRECTION__DOWN) {
                direction  = DOWN;
            }

            // check if valid lizard
            if (lizard_movement_ch_pos != -1) {
                pos_x = lizards_data[lizard_movement_ch_pos].pos_x;
                pos_y = lizards_data[lizard_movement_ch_pos].pos_y;
                ch = lizards_data[lizard_movement_ch_pos].ch;

                zmq_send_FieldUpdate(field_update_pusher, pos_x, pos_y, ' ', -1, -1);

                /* calculates new mark position */
                res = pthread_mutex_lock(&roaches_mutex);
                assert(res == 0);
                res = pthread_mutex_lock(&wasps_mutex);
                assert(res == 0);

                int mov_res = new_position(&pos_x, &pos_y, direction, lizards_data, MAX_LIZARDS, roaches_data, MAX_ROACHES_WASPS, wasps_data, MAX_ROACHES_WASPS);
                res = pthread_mutex_unlock(&wasps_mutex);
                assert(res == 0);
                lizards_data[lizard_movement_ch_pos].pos_x = pos_x;
                lizards_data[lizard_movement_ch_pos].pos_y = pos_y;

                // collision with lizard
                if (mov_res >= 'a' && mov_res <= 'z') { 
                    int lizard_pos = find_ch_info(lizards_data, MAX_LIZARDS, mov_res);
                    int average = (lizards_data[lizard_movement_ch_pos].score + lizards_data[lizard_pos].score) / 2;
                    lizards_data[lizard_movement_ch_pos].score = average;
                    lizards_data[lizard_pos].score = average;

                    zmq_send_FieldUpdate(field_update_pusher, pos_x, pos_y, ch, lizards_data[lizard_pos].ch, average);
                }
                // collision with roach
                else if (mov_res >= 'z' + 1 && mov_res < 'z' + 1 + MAX_ROACHES_WASPS) { 
                    int roach_pos = find_ch_info(roaches_data, MAX_ROACHES_WASPS, mov_res);
                    int roach_x = roaches_data[roach_pos].pos_x;
                    int roach_y = roaches_data[roach_pos].pos_y;

                    //check if roaches are stacked
                    for (int i = 0; i < MAX_ROACHES_WASPS; i++) {
                        if (roaches_data[i].pos_x == roach_x && roaches_data[i].pos_y == roach_y && roaches_data[i].ch != -1 && roaches_data[i].timestamp == 0) {
                            lizards_data[lizard_movement_ch_pos].score += roaches_data[i].score;
                            roaches_data[i].timestamp = time(NULL);
                        }
                    }
                }
                // collision with wasp
                else if (mov_res != 0) {
                    lizards_data[lizard_movement_ch_pos].score -= 10;
                }
                
                zmq_send_FieldUpdate(field_update_pusher, pos_x, pos_y, ch, ch, lizards_data[lizard_movement_ch_pos].score);

                //delete old tail
                res = pthread_mutex_lock(&wasps_mutex);
                assert(res == 0);
                for (int i = 0; i < lizards_data[lizard_movement_ch_pos].tail_size; i++) {
                    int tail_x = lizards_data[lizard_movement_ch_pos].body_x[i];
                    int tail_y = lizards_data[lizard_movement_ch_pos].body_y[i];
                    if (no_ch_in_pos(lizards_data, MAX_LIZARDS, tail_x, tail_y) && no_ch_in_pos(roaches_data, MAX_ROACHES_WASPS, tail_x, tail_y) && no_ch_in_pos(wasps_data, MAX_ROACHES_WASPS, tail_x, tail_y)) {

                        zmq_send_FieldUpdate(field_update_pusher, tail_x, tail_y, ' ', -1, -1);
                    }
                }
                res = pthread_mutex_unlock(&wasps_mutex);
                assert(res == 0);
                res = pthread_mutex_unlock(&roaches_mutex);
                assert(res == 0);
                // figure out new tail size and direction
                int dx = 0;
                int dy = 0;
                switch (direction) {
                    case LEFT:
                        lizards_data[lizard_movement_ch_pos].tail_size = min(5, WINDOW_SIZE - 2 - pos_y);
                        dy = 1;
                        break;
                    case RIGHT:
                        lizards_data[lizard_movement_ch_pos].tail_size = min(5, pos_y - 1);
                        dy = -1;
                        break;
                    case DOWN:
                        lizards_data[lizard_movement_ch_pos].tail_size = min(5, pos_x - 1);
                        dx = -1;
                        break;
                    case UP:
                        lizards_data[lizard_movement_ch_pos].tail_size = min(5, WINDOW_SIZE - 2 - pos_x);
                        dx = 1;
                        break;
                }
                if (lizards_data[lizard_movement_ch_pos].score >= 0) {
                    //draw new tail    
                    res = pthread_mutex_lock(&roaches_mutex);
                    assert(res == 0);
                    res = pthread_mutex_lock(&wasps_mutex);
                    assert(res == 0);
                    for (int i = 1; i < lizards_data[lizard_movement_ch_pos].tail_size + 1; i++) {
                        if (no_ch_in_pos(lizards_data, MAX_LIZARDS, pos_x + dx*i, pos_y + dy*i) && no_ch_in_pos(roaches_data, MAX_ROACHES_WASPS, pos_x + dx*i, pos_y + dy*i) && no_ch_in_pos(wasps_data, MAX_ROACHES_WASPS, pos_x + dx*i, pos_y + dy*i)) {
                            lizards_data[lizard_movement_ch_pos].body_x[i-1] = pos_x + dx*i;
                            lizards_data[lizard_movement_ch_pos].body_y[i-1] = pos_y + dy*i;
                            if (lizards_data[lizard_movement_ch_pos].score >= 50) {
                                zmq_send_FieldUpdate(field_update_pusher, pos_x + dx*i, pos_y + dy*i, '*', -1, -1);
                            }
                            else {
                                zmq_send_FieldUpdate(field_update_pusher, pos_x + dx*i, pos_y + dy*i, '.', -1, -1);
                            }
                        }
                    }
                    res = pthread_mutex_unlock(&wasps_mutex);
                    assert(res == 0);
                    res = pthread_mutex_unlock(&roaches_mutex);
                    assert(res == 0);
                }

                //generate new nonce
                int new_nonce = generate_nonce(0);
                lizards_data[lizard_movement_ch_pos].nonce = new_nonce;

                // response: success or fail + score + new nonce
                zmq_send_LizardMovementResp(responder, RESPONSE_SUCCESS, lizards_data[lizard_movement_ch_pos].score, new_nonce);

                res = pthread_mutex_unlock(&lizards_mutex);
                assert(res == 0);
            }
            break;
        case LIZARD_DISCONNECT:
            LizardDisconnectReq * pb_m_stuct_lizard_disconnect = zmq_read_LizardDisconnectReq(responder);

            res = pthread_mutex_lock(&lizards_mutex);
            assert(res == 0);

            int lizard_disconnect_ch_pos = find_ch_info(lizards_data, MAX_LIZARDS, pb_m_stuct_lizard_disconnect->character.data[0]);

            // check if valid lizard and nonce
            if (lizard_disconnect_ch_pos == -1 || lizards_data[lizard_disconnect_ch_pos].nonce != pb_m_stuct_lizard_disconnect->nonce) {
                zmq_send_LizardDisconnectResp(responder, RESPONSE_FAIL);
                break;
            }

            pos_x = lizards_data[lizard_disconnect_ch_pos].pos_x;
            pos_y = lizards_data[lizard_disconnect_ch_pos].pos_y;
            lizards_data[lizard_disconnect_ch_pos].ch = -1;
            n_lizards--;

            ///delete head
            zmq_send_FieldUpdate(field_update_pusher, pos_x, pos_y, ' ', lizards_data[lizard_disconnect_ch_pos].ch, INT_MIN);

            //delete tail
            res = pthread_mutex_lock(&roaches_mutex);
            assert(res == 0);
            res = pthread_mutex_lock(&wasps_mutex);
            assert(res == 0);
            for (int i = 0; i < lizards_data[lizard_disconnect_ch_pos].tail_size; i++) {
                int tail_x = lizards_data[lizard_disconnect_ch_pos].body_x[i];
                int tail_y = lizards_data[lizard_disconnect_ch_pos].body_y[i];
                if (no_ch_in_pos(lizards_data, MAX_LIZARDS, tail_x, tail_y) && no_ch_in_pos(roaches_data, MAX_ROACHES_WASPS, tail_x, tail_y)  && no_ch_in_pos(wasps_data, MAX_ROACHES_WASPS, tail_x, tail_y)) {
                    
                    zmq_send_FieldUpdate(field_update_pusher, tail_x, tail_y, ' ', -1, -1);
                }
            }
            res = pthread_mutex_unlock(&wasps_mutex);
            assert(res == 0);
            res = pthread_mutex_unlock(&roaches_mutex);
            assert(res == 0);
            
            zmq_send_LizardDisconnectResp(responder, RESPONSE_SUCCESS);

            res = pthread_mutex_unlock(&lizards_mutex);
            assert(res == 0);
            break;
        default:
            break;
        }
        free(msg_type);
    }
}

void * roaches_wasps_thread_function(void *args) {
    void *responder = zmq_socket (context, ZMQ_REP);
    int rc = zmq_bind(responder, "tcp://*:5556");
    assert(rc == 0);

    void *field_update_pusher = zmq_socket (context, ZMQ_PUSH);
    rc = zmq_connect(field_update_pusher, "inproc://field_update");
    assert (rc == 0);

    int ch;
    int pos_x;
    int pos_y;
    int nonce;
    direction_t  direction;

    while (1) {

        char *msg_type = s_recv(responder);
        assert(msg_type != NULL);

        switch (msg_type[0]) {
        case ROACHES_CONNECT:
            int roaches_characters[10];
            int roaches_nonces[10];
            RoachesConnectReq * pb_m_stuct_roaches_connect = zmq_read_RoachesConnectReq(responder);

            int res = pthread_mutex_lock(&lizards_mutex);
            assert(res == 0);
            res = pthread_mutex_lock(&roaches_mutex);
            assert(res == 0);
            res = pthread_mutex_lock(&wasps_mutex);
            assert(res == 0);

            // if full of roaches and wasps deny request
            if (n_roaches + pb_m_stuct_roaches_connect->number + n_wasps > MAX_ROACHES_WASPS) {
                zmq_send_RoachesConnectResp(responder, RESPONSE_FAIL, -1, roaches_characters, roaches_nonces);
                res = pthread_mutex_unlock(&wasps_mutex);
                assert(res == 0);
                res = pthread_mutex_unlock(&roaches_mutex);
                assert(res == 0);
                res = pthread_mutex_unlock(&lizards_mutex);
                assert(res == 0);
                break;
            }

            int roach_score;

            // generate ids and positions for all roaches
            for (int i = 0; i < pb_m_stuct_roaches_connect->number; i++) {
                nonce = generate_nonce(i);
                roach_score = pb_m_stuct_roaches_connect->scores[i];
                ch = random_unused_character(roaches_data, MAX_ROACHES_WASPS, 'z' + 1, MAX_ROACHES_WASPS);
                random_empty_position(lizards_data, MAX_LIZARDS, roaches_data, MAX_ROACHES_WASPS, wasps_data, MAX_ROACHES_WASPS, &pos_x, &pos_y);

                int roach_connect_ch_pos;
                for (int i = 0; i < MAX_ROACHES_WASPS; i++) {
                    if (roaches_data[i].ch == -1) {
                        roach_connect_ch_pos = i;
                        break;
                    }
                }
                roaches_data[roach_connect_ch_pos].ch = ch;
                roaches_data[roach_connect_ch_pos].pos_x = pos_x;
                roaches_data[roach_connect_ch_pos].pos_y = pos_y;
                roaches_data[roach_connect_ch_pos].score = roach_score;
                roaches_data[roach_connect_ch_pos].nonce = nonce;
                roaches_nonces[i] = nonce;
                roaches_characters[i] = ch;
                n_roaches++;

                zmq_send_FieldUpdate(field_update_pusher, pos_x, pos_y, roach_score + '0', -1, -1);
            }
            res = pthread_mutex_unlock(&wasps_mutex);
            assert(res == 0);
            res = pthread_mutex_unlock(&roaches_mutex);
            assert(res == 0);
            res = pthread_mutex_unlock(&lizards_mutex);
            assert(res == 0);

            // response: success or fail + ids of roaches + nonce
            zmq_send_RoachesConnectResp(responder, RESPONSE_SUCCESS, pb_m_stuct_roaches_connect->number, roaches_characters, roaches_nonces);

            send_board(field_update_pusher, lizards_data, MAX_LIZARDS, roaches_data, MAX_ROACHES_WASPS, wasps_data, MAX_ROACHES_WASPS);
            break;

        case ROACHES_MOVEMENT:
            RoachesMovementReq * pb_m_stuct_roaches_movement = zmq_read_RoachesMovementReq(responder);

            res = pthread_mutex_lock(&lizards_mutex);
            assert(res == 0);
            res = pthread_mutex_lock(&roaches_mutex);
            assert(res == 0);
            res = pthread_mutex_lock(&wasps_mutex);
            assert(res == 0);

            int roaches_movement_ch_pos = find_ch_info(roaches_data, MAX_ROACHES_WASPS, pb_m_stuct_roaches_movement->character);
            
            //check nonce
            if (roaches_data[roaches_movement_ch_pos].nonce != pb_m_stuct_roaches_movement->nonce) {
                zmq_send_RoachesMovementResp(responder, RESPONSE_FAIL, -1);
                res = pthread_mutex_unlock(&wasps_mutex);
                assert(res == 0);
                res = pthread_mutex_unlock(&roaches_mutex);
                assert(res == 0);
                res = pthread_mutex_unlock(&lizards_mutex);
                assert(res == 0);
            break;
            }

            if (pb_m_stuct_roaches_movement->direction == DIRECTION__LEFT) {
                direction = LEFT;
            }
            else if (pb_m_stuct_roaches_movement->direction == DIRECTION__RIGHT) {
                direction = RIGHT;
            }
            else if (pb_m_stuct_roaches_movement->direction == DIRECTION__UP) {
                direction = UP;
            }
            else if (pb_m_stuct_roaches_movement->direction == DIRECTION__DOWN) {
                direction  = DOWN;
            }

            // check if roach is valid and not dead
            if (roaches_movement_ch_pos != -1 && roaches_data[roaches_movement_ch_pos].timestamp == 0) {
                pos_x = roaches_data[roaches_movement_ch_pos].pos_x;
                pos_y = roaches_data[roaches_movement_ch_pos].pos_y;
                ch = roaches_data[roaches_movement_ch_pos].ch;

                zmq_send_FieldUpdate(field_update_pusher, pos_x, pos_y, ' ', -1, -1);

                // check for a roach below
                for (int i = 0; i < MAX_ROACHES_WASPS; i++) {
                    if (roaches_data[i].pos_x == pos_x && roaches_data[i].pos_y == pos_y && roaches_data[i].ch != ch && roaches_data[i].ch != -1 && roaches_data[i].timestamp == 0) {
                        zmq_send_FieldUpdate(field_update_pusher, pos_x, pos_y, roaches_data[i].score + '0', -1, -1);
                        break;
                    }
                }

                /* calculates new mark position */
                new_position(&pos_x, &pos_y, direction, lizards_data, MAX_LIZARDS, roaches_data, MAX_ROACHES_WASPS, wasps_data, MAX_ROACHES_WASPS);
                roaches_data[roaches_movement_ch_pos].pos_x = pos_x;
                roaches_data[roaches_movement_ch_pos].pos_y = pos_y;

                zmq_send_FieldUpdate(field_update_pusher, pos_x, pos_y, roaches_data[roaches_movement_ch_pos].score + '0', -1, -1);
            }
            
            //generate new nonce
            int new_nonce = generate_nonce(0);
            roaches_data[roaches_movement_ch_pos].nonce = new_nonce;

            res = pthread_mutex_unlock(&wasps_mutex);
            assert(res == 0);
            res = pthread_mutex_unlock(&roaches_mutex);
            assert(res == 0);
            res = pthread_mutex_unlock(&lizards_mutex);
            assert(res == 0);

            // response: success or fail + new nonce
            zmq_send_RoachesMovementResp(responder, RESPONSE_SUCCESS, new_nonce);

            break;
        case ROACHES_DISCONNECT:
            RoachesDisconnectReq * pb_m_stuct_roaches_disconnect = zmq_read_RoachesDisconnectReq(responder);
            
            res = pthread_mutex_lock(&roaches_mutex);
            assert(res == 0);

            int bad_roach = 0;
            for (int i = 0; i < pb_m_stuct_roaches_disconnect->number; i++) {
                int roach_disconnect_ch_pos = find_ch_info(roaches_data, MAX_ROACHES_WASPS, pb_m_stuct_roaches_disconnect->ids[i]);

                // check if valid roach and nonce
                if (roach_disconnect_ch_pos == -1 || roaches_data[roach_disconnect_ch_pos].nonce != pb_m_stuct_roaches_disconnect->nonces[i]) {
                    bad_roach = 1;
                    break;
                }

                pos_x = roaches_data[roach_disconnect_ch_pos].pos_x;
                pos_y = roaches_data[roach_disconnect_ch_pos].pos_y;
                ch = roaches_data[roach_disconnect_ch_pos].ch;
                roaches_data[roach_disconnect_ch_pos].ch = -1;
                n_roaches--;

                zmq_send_FieldUpdate(field_update_pusher, pos_x, pos_y, ' ', -1, -1);

                // check for a roach below
                for (int j = 0; j < MAX_ROACHES_WASPS; j++) {
                    if (roaches_data[j].pos_x == pos_x && roaches_data[j].pos_y == pos_y && roaches_data[j].ch != ch && roaches_data[j].ch != -1 && roaches_data[i].timestamp == 0) {
                        zmq_send_FieldUpdate(field_update_pusher, roaches_data[j].pos_x, roaches_data[j].pos_y, roaches_data[j].score + '0', -1, -1);
                        break;
                    }
                }
            }

            res = pthread_mutex_unlock(&roaches_mutex);
            assert(res == 0);

            if (!bad_roach)
                zmq_send_RoachesDisconnectResp(responder, RESPONSE_SUCCESS);
            else
                zmq_send_RoachesDisconnectResp(responder, RESPONSE_FAIL);

            break;

        case WASPS_DISCONNECT:
            WaspsDisconnectReq * pb_m_stuct_wasps_disconnect = zmq_read_WaspsDisconnectReq(responder);
            
            res = pthread_mutex_lock(&wasps_mutex);
            assert(res == 0);

            int bad_wasp = 0;
            for (int i = 0; i < pb_m_stuct_wasps_disconnect->number; i++) {
                int wasp_disconnect_ch_pos = find_ch_info(wasps_data, MAX_ROACHES_WASPS, pb_m_stuct_wasps_disconnect->ids[i]);

                // check if valid wasp and nonce
                if (wasp_disconnect_ch_pos == -1 || wasps_data[wasp_disconnect_ch_pos].nonce != pb_m_stuct_wasps_disconnect->nonces[i]) {
                    bad_wasp = 1;
                    break;
                }

                pos_x = wasps_data[wasp_disconnect_ch_pos].pos_x;
                pos_y = wasps_data[wasp_disconnect_ch_pos].pos_y;
                wasps_data[wasp_disconnect_ch_pos].ch = -1;
                n_wasps--;

                zmq_send_FieldUpdate(field_update_pusher, pos_x, pos_y, ' ', -1, -1);
            }

            res = pthread_mutex_unlock(&wasps_mutex);
            assert(res == 0);

            if (!bad_wasp)
                zmq_send_WaspsDisconnectResp(responder, RESPONSE_SUCCESS);
            else
                zmq_send_WaspsDisconnectResp(responder, RESPONSE_FAIL);

            break;

        case WASPS_CONNECT:
            int wasps_characters[10];
            int wasps_nonces[10];
            WaspsConnectReq * pb_m_stuct_wasps_connect = zmq_read_WaspsConnectReq(responder);

            res = pthread_mutex_lock(&lizards_mutex);
            assert(res == 0);
            res = pthread_mutex_lock(&roaches_mutex);
            assert(res == 0);
            res = pthread_mutex_lock(&wasps_mutex);
            assert(res == 0);

            // if full of roaches and wasps deny request
            if (n_roaches + pb_m_stuct_wasps_connect->number + n_wasps > MAX_ROACHES_WASPS) {
                zmq_send_WaspsConnectResp(responder, RESPONSE_FAIL, -1, wasps_characters, wasps_nonces);
                res = pthread_mutex_unlock(&wasps_mutex);
                assert(res == 0);
                res = pthread_mutex_unlock(&roaches_mutex);
                assert(res == 0);
                res = pthread_mutex_unlock(&lizards_mutex);
                assert(res == 0);
                break;
            }

            // generate ids and positions for all wasps
            for (int i = 0; i < pb_m_stuct_wasps_connect->number; i++) {
                nonce = generate_nonce(i);
                ch = random_unused_character(wasps_data, n_wasps, 'z' + 1 + MAX_ROACHES_WASPS, MAX_ROACHES_WASPS);
                random_empty_position(lizards_data, MAX_LIZARDS, roaches_data, MAX_ROACHES_WASPS, wasps_data, MAX_ROACHES_WASPS, &pos_x, &pos_y);

                int wasp_connect_ch_pos;
                for (int i = 0; i < MAX_ROACHES_WASPS; i++) {
                    if (wasps_data[i].ch == -1) {
                        wasp_connect_ch_pos = i;
                        break;
                    }
                }
                wasps_data[wasp_connect_ch_pos].ch = ch;
                wasps_data[wasp_connect_ch_pos].pos_x = pos_x;
                wasps_data[wasp_connect_ch_pos].pos_y = pos_y;
                wasps_data[wasp_connect_ch_pos].score = -10;
                wasps_data[wasp_connect_ch_pos].nonce = nonce;
                wasps_nonces[i] = nonce;
                wasps_characters[i] = ch;
                n_wasps++;

                zmq_send_FieldUpdate(field_update_pusher, pos_x, pos_y, '#', -1, -1);
            }

            res = pthread_mutex_unlock(&wasps_mutex);
            assert(res == 0);
            res = pthread_mutex_unlock(&roaches_mutex);
            assert(res == 0);
            res = pthread_mutex_unlock(&lizards_mutex);
            assert(res == 0);

            // response: success or fail + ids of wasps + nonce
            zmq_send_WaspsConnectResp(responder, RESPONSE_SUCCESS, pb_m_stuct_wasps_connect->number, wasps_characters, wasps_nonces);

            send_board(field_update_pusher, lizards_data, MAX_LIZARDS, roaches_data, MAX_ROACHES_WASPS, wasps_data, MAX_ROACHES_WASPS);
            break;

        case WASPS_MOVEMENT:
            WaspsMovementReq * pb_m_stuct_wasps_movement = zmq_read_WaspsMovementReq(responder);

            res = pthread_mutex_lock(&lizards_mutex);
            assert(res == 0);
            res = pthread_mutex_lock(&roaches_mutex);
            assert(res == 0);
            res = pthread_mutex_lock(&wasps_mutex);
            assert(res == 0);

            int wasp_movement_ch_pos = find_ch_info(wasps_data, MAX_ROACHES_WASPS, pb_m_stuct_wasps_movement->character);

            // check the nonce
            if (wasps_data[wasp_movement_ch_pos].nonce != pb_m_stuct_wasps_movement->nonce) {
                zmq_send_WaspsMovementResp(responder, RESPONSE_FAIL, -1);
                res = pthread_mutex_unlock(&wasps_mutex);
                assert(res == 0);
                res = pthread_mutex_unlock(&roaches_mutex);
                assert(res == 0);
                res = pthread_mutex_unlock(&lizards_mutex);
                assert(res == 0);
                break;
            }

            if (pb_m_stuct_wasps_movement->direction == DIRECTION__LEFT) {
                direction = LEFT;
            }
            else if (pb_m_stuct_wasps_movement->direction == DIRECTION__RIGHT) {
                direction = RIGHT;
            }
            else if (pb_m_stuct_wasps_movement->direction == DIRECTION__UP) {
                direction = UP;
            }
            else if (pb_m_stuct_wasps_movement->direction == DIRECTION__DOWN) {
                direction  = DOWN;
            }

            // check if valid wasp
            if (wasp_movement_ch_pos != -1) {
                pos_x = wasps_data[wasp_movement_ch_pos].pos_x;
                pos_y = wasps_data[wasp_movement_ch_pos].pos_y;
                ch = wasps_data[wasp_movement_ch_pos].ch;

                zmq_send_FieldUpdate(field_update_pusher, pos_x, pos_y, ' ', -1, -1);

                int old_pos_x = pos_x;
                int old_pos_y = pos_y;

                /* calculates new mark position */
                int mov_res = new_position(&pos_x, &pos_y, direction, lizards_data, MAX_LIZARDS, roaches_data, MAX_ROACHES_WASPS, wasps_data, MAX_ROACHES_WASPS);
                wasps_data[wasp_movement_ch_pos].pos_x = pos_x;
                wasps_data[wasp_movement_ch_pos].pos_y = pos_y;

                // collision with lizard
                if (mov_res >= 'a' && mov_res <= 'z') { 
                    int lizard_pos = find_ch_info(lizards_data, MAX_LIZARDS, mov_res);
                    lizards_data[lizard_pos].score -= 10;

                    zmq_send_FieldUpdate(field_update_pusher, pos_x, pos_y, '#', lizards_data[lizard_pos].ch, lizards_data[lizard_pos].score);
                }
                // collision with roach
                else if (mov_res >= 'z' + 1 && mov_res < 'z' + 1 + MAX_ROACHES_WASPS) { 
                    pos_x = old_pos_x;
                    pos_y = old_pos_y;
                    wasps_data[wasp_movement_ch_pos].pos_x = pos_x;
                    wasps_data[wasp_movement_ch_pos].pos_y = pos_y;
                }

                zmq_send_FieldUpdate(field_update_pusher, pos_x, pos_y, '#', -1, -1);

                //generate new nonce
                int new_nonce = generate_nonce(0);
                wasps_data[wasp_movement_ch_pos].nonce = new_nonce;

                res = pthread_mutex_unlock(&wasps_mutex);
                assert(res == 0);
                res = pthread_mutex_unlock(&roaches_mutex);
                assert(res == 0);
                res = pthread_mutex_unlock(&lizards_mutex);
                assert(res == 0);
                // response: success or fail + score + new nonce
                zmq_send_WaspsMovementResp(responder, RESPONSE_SUCCESS, new_nonce);
            }
            break;

        default:
            break;
        }
        free(msg_type);

        // check timestamps to respawn roaches
        int res = pthread_mutex_lock(&lizards_mutex);
        assert(res == 0);
        res = pthread_mutex_lock(&roaches_mutex);
        assert(res == 0);
        res = pthread_mutex_lock(&wasps_mutex);
        assert(res == 0);
        time_t currentTime = time(NULL);
        for (int i = 0; i < MAX_ROACHES_WASPS; i++) {
            if (roaches_data[i].timestamp != 0 && currentTime - roaches_data[i].timestamp >= 5) {
                // respawn
                int new_pos_x;
                int new_pos_y;
                // get new position
                random_empty_position(lizards_data, MAX_LIZARDS, roaches_data, MAX_ROACHES_WASPS, wasps_data, MAX_ROACHES_WASPS, &new_pos_x, &new_pos_y);
                roaches_data[i].pos_x = new_pos_x;
                roaches_data[i].pos_y = new_pos_y;
                // reset timestamp
                roaches_data[i].timestamp = 0;
                // draw
                zmq_send_FieldUpdate(field_update_pusher, new_pos_x, new_pos_y, roaches_data[i].score + '0', -1, -1);
            }
        }
        res = pthread_mutex_unlock(&wasps_mutex);
        assert(res == 0);
        res = pthread_mutex_unlock(&roaches_mutex);
        assert(res == 0);
        res = pthread_mutex_unlock(&lizards_mutex);
        assert(res == 0);
    }
}

void * field_update_thread_function(void *args) {
    // Bind a ZMQ_PUB socket
    void *publisher = zmq_socket (context, ZMQ_PUB);
    int rc = zmq_bind (publisher, "tcp://*:5557");
    assert(rc == 0);

    //  Socket facing subscribers
    void *field_update_socket = zmq_socket(context, ZMQ_PULL);
    rc = zmq_bind(field_update_socket, "inproc://field_update");
    assert (rc == 0);

    // subscribe to topics
    zmq_setsockopt(field_update_socket, ZMQ_SUBSCRIBE, "field_update", 13);

	initscr();		    	
	cbreak();				
    keypad(stdscr, TRUE);   
	noecho();

    /* creates a window and draws a border */
    WINDOW * my_win = newwin(WINDOW_SIZE, WINDOW_SIZE, 0, 0);
    box(my_win, 0 , 0);	
	wrefresh(my_win);
    
    char *subscription;

    while (1) {
        // receive char and position
        subscription = s_recv(field_update_socket);
        assert(subscription != NULL);
        FieldUpdate * pb_m_stuct_field_update = zmq_read_FieldUpdate(field_update_socket);

        /* draw mark on new position */
        char ch = pb_m_stuct_field_update->character.data[0];
        wmove(my_win, pb_m_stuct_field_update->x, pb_m_stuct_field_update->y);
        waddch(my_win, ch| A_BOLD);
        wrefresh(my_win);

        int id = -1;
        int new_score = -1;
        if (pb_m_stuct_field_update->has_id) {
            id = pb_m_stuct_field_update->id.data[0];
            new_score = pb_m_stuct_field_update->new_score;
        }
        zmq_send_FieldUpdate(publisher, pb_m_stuct_field_update->x, pb_m_stuct_field_update->y, ch, id, new_score);
    }

  	endwin();			/* End curses mode */
}

int main()
{	
    for (int i = 0; i < MAX_LIZARDS; i++) {
        lizards_data[i].ch = -1;
        lizards_data[i].timestamp = 0;
    }

    for (int i = 0; i < MAX_ROACHES_WASPS; i++) {
        roaches_data[i].ch = -1;
        roaches_data[i].timestamp = 0;
    }

    for (int i = 0; i < MAX_ROACHES_WASPS; i++) {
        wasps_data[i].ch = -1;
    }

    context = zmq_ctx_new ();

    void *lizards_frontend = zmq_socket(context, ZMQ_ROUTER);
    int rc = zmq_bind(lizards_frontend, "tcp://*:5555");
    assert(rc == 0);

    void *lizards_backend = zmq_socket (context, ZMQ_DEALER);
    rc = zmq_bind(lizards_backend, "inproc://lizards_backend");
    assert(rc == 0);

    for (int i = 0; i < 4; i++) {
        pthread_t lizard_thread_id;
        pthread_create(&lizard_thread_id, NULL, lizard_thread_function, NULL);
    }

    pthread_t roaches_wasps_thread_id;
    pthread_create(&roaches_wasps_thread_id, NULL, roaches_wasps_thread_function, NULL);

    pthread_t field_update_thread_id;
    pthread_create(&field_update_thread_id, NULL, field_update_thread_function, NULL);

    zmq_proxy(lizards_frontend, lizards_backend, NULL);

	return 0;
}