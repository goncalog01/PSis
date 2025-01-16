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
#include <time.h>


#define WINDOW_SIZE 30
#define MAX_LIZARDS 26
#define MAX_ROACHES ((WINDOW_SIZE - 2)*(WINDOW_SIZE - 2))/3
#define RANDOM_POS (1 + random() % (WINDOW_SIZE - 2))
#define min(a, b) ((a) < (b) ? (a) : (b))

#define LIZARD_CONNECT 1
#define LIZARD_MOVEMENT 2
#define ROACHES_CONNECT 3
#define ROACHES_MOVEMENT 4
#define DISCONNECT 5
#define RESPONSE_SUCCESS 7
#define RESPONSE_FAIL 8

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

int new_position(int *x, int *y, direction_t direction, ch_info_t lizards_data[], int n_lizards, ch_info_t roaches_data[], int n_roaches) {
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
        if (lizards_data[i].pos_x == new_x && lizards_data[i].pos_y == new_y) {
            return lizards_data[i].ch;
        }
    }
    // check for collision with roach
    for (int i = 0; i < n_roaches; i++) {
        if (roaches_data[i].pos_x == new_x && roaches_data[i].pos_y == new_y) {
            *x = new_x;
            *y = new_y;
            return roaches_data[i].ch;
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

void random_empty_position(ch_info_t lizards_data[], int n_lizards, ch_info_t roaches_data[], int n_roaches, int *pos_x, int *pos_y) {
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

// To send updates to the display apps
void publish(void *publisher, int ch, int x, int y) {
    int nbytes = s_sendmore(publisher, "field_update");
    assert(nbytes != -1);
    char character[2];
    character[0] = ch;
    character[1] = '\0';
    nbytes = s_sendmore(publisher, character);
    assert(nbytes != -1);
    char pos_x[2];
    pos_x[0] = x;
    pos_x[1] = '\0';
    nbytes = s_sendmore(publisher, pos_x);
    assert(nbytes != -1);
    char pos_y[2];
    pos_y[0] = y;
    pos_y[1] = '\0';
    nbytes = s_send(publisher, pos_y);
    assert(nbytes != -1);
}

// Function to generate a random nonce
int generate_nonce(int i) {
    // Seed the random number generator with the current time
    srand((unsigned int)time(NULL) + i);

    // Generate a random number as the nonce
    int nonce = rand();

    return nonce;
}

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

int main()
{	
    
    ch_info_t lizards_data[MAX_LIZARDS];
    ch_info_t roaches_data[MAX_ROACHES];
    int n_lizards = 0;
    int n_roaches = 0;
    remote_char_t m;

    for (int i = 0; i < MAX_LIZARDS; i++) {
        lizards_data[i].ch = -1;
        lizards_data[i].timestamp = 0;
    }

    for (int i = 0; i < MAX_ROACHES; i++) {
        roaches_data[i].timestamp = 0;
    }

    void *context = zmq_ctx_new ();
    // Bind a ZMQ_REP socket
    void *responder = zmq_socket (context, ZMQ_REP);
    int rc = zmq_bind (responder, "tcp://*:5555");
    assert(rc == 0);
    
    // Bind a ZMQ_PUB socket
    void *publisher = zmq_socket (context, ZMQ_PUB);
    rc = zmq_bind (publisher, "tcp://*:5556");
    assert(rc == 0);

	initscr();		    	
	cbreak();				
    keypad(stdscr, TRUE);   
	noecho();			    

    /* creates a window and draws a border */
    WINDOW * my_win = newwin(WINDOW_SIZE, WINDOW_SIZE, 0, 0);
    box(my_win, 0 , 0);	
	wrefresh(my_win);

    int ch;
    int pos_x;
    int pos_y;
    int nonce;
    direction_t  direction;

    while (1) {

        char *msg_type = s_recv(responder);
        assert(msg_type != NULL);

        int nbytes;

        switch (msg_type[0]) {
        case LIZARD_CONNECT:
            char lizard_connect_response_type[2];
            lizard_connect_response_type[1] = '\0';

            // if full of lizards deny request
            if (n_lizards == MAX_LIZARDS) {
                lizard_connect_response_type[0] = RESPONSE_FAIL;
                nbytes = s_send(responder, lizard_connect_response_type);
                assert(nbytes != -1);
                break;
            }

            //assign random char and place it at random position
            ch = random_unused_character(lizards_data, MAX_LIZARDS, 'a', MAX_LIZARDS);
            random_empty_position(lizards_data, MAX_LIZARDS, roaches_data, n_roaches, &pos_x, &pos_y);
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

            /* draw mark on new position */
            wmove(my_win, pos_x, pos_y);
            waddch(my_win,ch| A_BOLD);

            publish(publisher, ch, pos_x, pos_y);

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
            for (int i = 1; i < lizards_data[lizard_connect_ch_pos].tail_size + 1; i++) {
                if (no_ch_in_pos(lizards_data, MAX_LIZARDS, pos_x + dx*i, pos_y + dy*i) && no_ch_in_pos(roaches_data, n_roaches, pos_x + dx*i, pos_y + dy*i)) {
                    lizards_data[lizard_connect_ch_pos].body_x[i-1] = pos_x + dx*i;
                    lizards_data[lizard_connect_ch_pos].body_y[i-1] = pos_y + dy*i;
                    wmove(my_win, pos_x + dx*i, pos_y + dy*i);
                    waddch(my_win,'.');
                    publish(publisher, '.', pos_x + dx*i, pos_y + dy*i);
                }
            }

            wrefresh(my_win);

            // response: success or fail + char of lizard + nonce
            lizard_connect_response_type[0] = RESPONSE_SUCCESS;
            char lizard_connect_response_character[2];
            lizard_connect_response_character[0] = ch;
            lizard_connect_response_character[1] = '\0';
            nbytes = s_sendmore(responder, lizard_connect_response_type);
            assert(nbytes != -1);
            nbytes = s_sendmore(responder, lizard_connect_response_character);
            assert(nbytes != -1);
            nbytes = send_integer(responder, nonce, 0);
            assert(nbytes != -1);
            break;

        case LIZARD_MOVEMENT:
            char *lizard_movement_request_character = s_recv(responder);
            assert(lizard_movement_request_character != NULL);
            char *lizard_movement_request_dir = s_recv(responder);
            assert(lizard_movement_request_dir != NULL);
            int lizard_movement_nonce = receive_integer(responder);
            assert(lizard_movement_nonce != -1);

            int lizard_movement_ch_pos = find_ch_info(lizards_data, MAX_LIZARDS, lizard_movement_request_character[0]);

            // check the nonce
            if (lizards_data[lizard_movement_ch_pos].nonce != lizard_movement_nonce) {
                free(lizard_movement_request_character);
                free(lizard_movement_request_dir);

                char response_type[2];
                response_type[1] = '\0';
                response_type[0] = RESPONSE_FAIL;
                nbytes = s_send(responder, response_type);
                assert(nbytes != -1);

                break;
            }

            if (strcmp(lizard_movement_request_dir, "LEFT") == 0) {
                direction = LEFT;
            }
            else if (strcmp(lizard_movement_request_dir, "RIGHT") == 0) {
                direction = RIGHT;
            }
            else if (strcmp(lizard_movement_request_dir, "UP") == 0) {
                direction = UP;
            }
            else if (strcmp(lizard_movement_request_dir, "DOWN") == 0) {
                direction  = DOWN;
            }
            free(lizard_movement_request_character);
            free(lizard_movement_request_dir);

            // check if valid lizard
            if (lizard_movement_ch_pos != -1) {
                pos_x = lizards_data[lizard_movement_ch_pos].pos_x;
                pos_y = lizards_data[lizard_movement_ch_pos].pos_y;
                ch = lizards_data[lizard_movement_ch_pos].ch;

                /* deletes old place */
                wmove(my_win, pos_x, pos_y);
                waddch(my_win,' ');

                publish(publisher, ' ', pos_x, pos_y);

                /* calculates new mark position */
                int res = new_position(&pos_x, &pos_y, direction, lizards_data, MAX_LIZARDS, roaches_data, n_roaches);
                lizards_data[lizard_movement_ch_pos].pos_x = pos_x;
                lizards_data[lizard_movement_ch_pos].pos_y = pos_y;

                // collision with lizard
                if (res >= 'a' && res <= 'z') { 
                    int lizard_pos = find_ch_info(lizards_data, MAX_LIZARDS, res);
                    int average = (lizards_data[lizard_movement_ch_pos].score + lizards_data[lizard_pos].score) / 2;
                    lizards_data[lizard_movement_ch_pos].score = average;
                    lizards_data[lizard_pos].score = average;
                }
                // collision with roach
                else if (res != 0) { 
                    int roach_pos = find_ch_info(roaches_data, n_roaches, res);
                    int roach_x = roaches_data[roach_pos].pos_x;
                    int roach_y = roaches_data[roach_pos].pos_y;

                    //check if roaches are stacked
                    for (int i = 0; i < n_roaches; i++) {
                        if (roaches_data[i].pos_x == roach_x && roaches_data[i].pos_y == roach_y) {
                            lizards_data[lizard_movement_ch_pos].score += roaches_data[i].score;
                            roaches_data[i].timestamp = time(NULL);
                        }
                    }
                }

                /* draw mark on new position */
                wmove(my_win, pos_x, pos_y);
                waddch(my_win,ch| A_BOLD);

                publish(publisher, ch, pos_x, pos_y);

                //delete old tail
                for (int i = 0; i < lizards_data[lizard_movement_ch_pos].tail_size; i++) {
                    int tail_x = lizards_data[lizard_movement_ch_pos].body_x[i];
                    int tail_y = lizards_data[lizard_movement_ch_pos].body_y[i];
                    if (no_ch_in_pos(lizards_data, MAX_LIZARDS, tail_x, tail_y) && no_ch_in_pos(roaches_data, n_roaches, tail_x, tail_y)) {
                        wmove(my_win, tail_x, tail_y);
                        waddch(my_win,' ');
                        publish(publisher, ' ', tail_x, tail_y);
                    }
                }
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
                //draw new tail
                for (int i = 1; i < lizards_data[lizard_movement_ch_pos].tail_size + 1; i++) {
                    if (no_ch_in_pos(lizards_data, MAX_LIZARDS, pos_x + dx*i, pos_y + dy*i) && no_ch_in_pos(roaches_data, n_roaches, pos_x + dx*i, pos_y + dy*i)) {
                        lizards_data[lizard_movement_ch_pos].body_x[i-1] = pos_x + dx*i;
                        lizards_data[lizard_movement_ch_pos].body_y[i-1] = pos_y + dy*i;
                        wmove(my_win, pos_x + dx*i, pos_y + dy*i);
                        if (lizards_data[lizard_movement_ch_pos].score >= 50) {
                            waddch(my_win,'*');
                            publish(publisher, '*', pos_x + dx*i, pos_y + dy*i);
                        }
                        else {
                            waddch(my_win,'.');
                            publish(publisher, '.', pos_x + dx*i, pos_y + dy*i);
                        }
                    }
                }

                wrefresh(my_win);

                // response: success or fail + score + new nonce
                char response_type[2];
                response_type[1] = '\0';
                response_type[0] = RESPONSE_SUCCESS;
                nbytes = s_sendmore(responder, response_type);
                assert(nbytes != -1);

                nbytes = send_integer(responder, lizards_data[lizard_movement_ch_pos].score, ZMQ_SNDMORE);
                assert(nbytes != -1);

                //generate new nonce
                int new_nonce = generate_nonce(0);
                lizards_data[lizard_movement_ch_pos].nonce = new_nonce;
                nbytes = send_integer(responder, new_nonce, 0);
                assert(nbytes != -1);
            }
            break;

        case ROACHES_CONNECT:
            int characters[11];
            int nonces[10];
            char *n_roaches_client = s_recv(responder);
            assert(n_roaches_client != NULL);

            char roaches_connect_response_type[2];
            roaches_connect_response_type[1] = '\0';

            // if full of roaches deny request
            if (n_roaches + n_roaches_client[0] > MAX_ROACHES) {
                roaches_connect_response_type[0] = RESPONSE_FAIL;
                free(n_roaches_client);
                nbytes = s_send(responder, roaches_connect_response_type);
                assert(nbytes != -1);
                break;
            }
            else {
                roaches_connect_response_type[0] = RESPONSE_SUCCESS;
            }

            char *roach_score;

            // generate ids and positions for all roaches
            for (int i = 0; i < n_roaches_client[0]; i++) {
                nonce = generate_nonce(i);
                roach_score = s_recv(responder);
                ch = random_unused_character(roaches_data, n_roaches, 'z' + 1, MAX_ROACHES);
                random_empty_position(lizards_data, MAX_LIZARDS, roaches_data, n_roaches, &pos_x, &pos_y);
                roaches_data[n_roaches].ch = ch;
                roaches_data[n_roaches].pos_x = pos_x;
                roaches_data[n_roaches].pos_y = pos_y;
                roaches_data[n_roaches].score = roach_score[0];
                roaches_data[n_roaches].nonce = nonce;
                nonces[i] = nonce;
                characters[i] = ch;
                n_roaches++;
                /* draw mark on new position */
                wmove(my_win, pos_x, pos_y);
                waddch(my_win,(roach_score[0] + '0')| A_BOLD);
                wrefresh(my_win);

                publish(publisher, roach_score[0] + '0', pos_x, pos_y);
            }
            characters[n_roaches_client[0]] = '\0';  

            // response: success or fail + ids of roaches + nonce
            nbytes = s_sendmore(responder, roaches_connect_response_type);
            assert(nbytes != -1);
            nbytes = zmq_send(responder, characters, 11*sizeof(int), ZMQ_SNDMORE);
            assert(nbytes != -1);
            nbytes = zmq_send(responder, nonces, 10*sizeof(int), 0);
            assert(nbytes != -1);

            free(n_roaches_client);
            free(roach_score);
            break;

        case ROACHES_MOVEMENT:
            int roach_char = receive_integer(responder);
            assert(roach_char != -1);
            char *dir = s_recv(responder);
            assert(dir != NULL);
            int roaches_movement_nonce = receive_integer(responder);
            assert(roaches_movement_nonce != -1);

            int roaches_movement_ch_pos = find_ch_info(roaches_data, n_roaches, roach_char);
            
            //check nonce
            if (roaches_data[roaches_movement_ch_pos].nonce != roaches_movement_nonce) {
                free(dir);

                char response_type[2];
                response_type[1] = '\0';
                response_type[0] = RESPONSE_FAIL;
                nbytes = s_send(responder, response_type);
                assert(nbytes != -1);

                break;
            }

            if (strcmp(dir, "LEFT") == 0) {
                direction = LEFT;
            }
            else if (strcmp(dir, "RIGHT") == 0) {
                direction = RIGHT;
            }
            else if (strcmp(dir, "UP") == 0) {
                direction = UP;
            }
            else if (strcmp(dir, "DOWN") == 0) {
                direction  = DOWN;
            }
            free(dir);

            // check if roach is valid and not dead
            if (roaches_movement_ch_pos != -1 && roaches_data[roaches_movement_ch_pos].timestamp == 0) {
                pos_x = roaches_data[roaches_movement_ch_pos].pos_x;
                pos_y = roaches_data[roaches_movement_ch_pos].pos_y;
                ch = roaches_data[roaches_movement_ch_pos].ch;

                /* deletes old place */
                wmove(my_win, pos_x, pos_y);
                waddch(my_win,' ');

                publish(publisher, ' ', pos_x, pos_y);

                // check for a roach below
                for (int i = 0; i < n_roaches; i++) {
                    if (roaches_data[i].pos_x == pos_x && roaches_data[i].pos_y == pos_y && roaches_data[i].ch != ch) {
                        /* draw below roach on old position */
                        wmove(my_win, pos_x, pos_y);
                        waddch(my_win,roaches_data[i].score + '0'| A_BOLD);
                        wrefresh(my_win);
                        publish(publisher, roaches_data[i].score + '0', pos_x, pos_y);
                        break;
                    }
                }

                /* calculates new mark position */
                int res = new_position(&pos_x, &pos_y, direction, lizards_data, MAX_LIZARDS, roaches_data, n_roaches);
                roaches_data[roaches_movement_ch_pos].pos_x = pos_x;
                roaches_data[roaches_movement_ch_pos].pos_y = pos_y;

                /* draw mark on new position */
                wmove(my_win, pos_x, pos_y);
                waddch(my_win,(roaches_data[roaches_movement_ch_pos].score + '0')| A_BOLD);
                wrefresh(my_win);

                publish(publisher, roaches_data[roaches_movement_ch_pos].score + '0', pos_x, pos_y);
            }

            // response: success or fail + new nonce
            char roaches_movement_response_type[2];
            roaches_movement_response_type[0] = RESPONSE_SUCCESS;
            roaches_movement_response_type[1] = '\0';
            nbytes = s_sendmore(responder, roaches_movement_response_type);
            assert(nbytes != -1);
            
            //generate new nonce
            int new_nonce = generate_nonce(0);
            roaches_data[roaches_movement_ch_pos].nonce = new_nonce;
            nbytes = send_integer(responder, new_nonce, 0);
            assert(nbytes != -1);

            break;
        case DISCONNECT:
            char *disconnect_request_character = s_recv(responder);
            assert(disconnect_request_character != NULL);
            int disconnect_request_nonce = receive_integer(responder);

            int disconnect_ch_pos = find_ch_info(lizards_data, MAX_LIZARDS, disconnect_request_character[0]);

            // check nonce
            if (lizards_data[disconnect_ch_pos].nonce != disconnect_request_nonce) {
                free(disconnect_request_character);

                char response_type[2];
                response_type[1] = '\0';
                response_type[0] = RESPONSE_FAIL;
                nbytes = s_send(responder, response_type);
                assert(nbytes != -1);

                break;
            }

            free(disconnect_request_character);

            // check if valid lizard
            if (disconnect_ch_pos != -1) {
                pos_x = lizards_data[disconnect_ch_pos].pos_x;
                pos_y = lizards_data[disconnect_ch_pos].pos_y;
                lizards_data[disconnect_ch_pos].ch = -1;

                ///delete head
                wmove(my_win, pos_x, pos_y);
                waddch(my_win,' ');

                publish(publisher, ' ', pos_x, pos_y);

                //delete tail
                for (int i = 0; i < lizards_data[disconnect_ch_pos].tail_size; i++) {
                    int tail_x = lizards_data[disconnect_ch_pos].body_x[i];
                    int tail_y = lizards_data[disconnect_ch_pos].body_y[i];
                    if (no_ch_in_pos(lizards_data, MAX_LIZARDS, tail_x, tail_y) && no_ch_in_pos(roaches_data, n_roaches, tail_x, tail_y)) {
                        wmove(my_win, tail_x, tail_y);
                        waddch(my_win,' ');
                        publish(publisher, ' ', tail_x, tail_y);
                    }
                }

                wrefresh(my_win);

                char disconnect_response_type[2];
                disconnect_response_type[0] = RESPONSE_SUCCESS;
                disconnect_response_type[1] = '\0';
                nbytes = s_send(responder, disconnect_response_type);
                assert(nbytes != -1);
            }
            break;
        default:
            break;
        }
        free(msg_type);

        // check timestamps to respawn roaches
        time_t currentTime = time(NULL);
        for (int i = 0; i < n_roaches; i++) {
            if (roaches_data[i].timestamp != 0 && currentTime - roaches_data[i].timestamp >= 5) {
                // respawn
                int new_pos_x;
                int new_pos_y;
                // get new position
                random_empty_position(lizards_data, MAX_LIZARDS, roaches_data, n_roaches, &new_pos_x, &new_pos_y);
                roaches_data[i].pos_x = new_pos_x;
                roaches_data[i].pos_y = new_pos_y;
                // reset timestamp
                roaches_data[i].timestamp = 0;
                // draw
                wmove(my_win, new_pos_x, new_pos_y);
                waddch(my_win,(roaches_data[i].score + '0')| A_BOLD);
                wrefresh(my_win);

                publish(publisher, roaches_data[i].score + '0', new_pos_x, new_pos_y);
            }
        }

    }

    zmq_close (responder);
    zmq_close (publisher);
    zmq_ctx_destroy (context);

  	endwin();			/* End curses mode */

	return 0;
}