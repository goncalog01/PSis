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
#include <ctype.h>

#define LIZARD_CONNECT 1
#define LIZARD_MOVEMENT 2
#define DISCONNECT 5
#define RESPONSE_SUCCESS 7
#define RESPONSE_FAIL 8

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
    
    char ch;
    int nonce;

    // connect message: message type
    int nbytes;
    char message_type[2];
    message_type[0] = LIZARD_CONNECT;
    message_type[1] = '\0';

    nbytes = s_send(requester, message_type);
    assert(nbytes != -1);

    // response: success or fail + char of lizard + nonce
    char *response_type = s_recv(requester);
    assert(response_type != NULL);
    if (response_type[0] == RESPONSE_FAIL) {
        free(response_type);
        exit(-1);
    }
    char *connect_character = s_recv(requester);
    assert(connect_character != NULL);
    ch = connect_character[0];
    int connect_nonce = receive_integer(requester);
    assert(connect_nonce != -1);
    nonce = connect_nonce;

    free(connect_character);
    free(response_type);

	initscr();			/* Start curses mode 		*/
	cbreak();				/* Line buffering disabled	*/
	keypad(stdscr, TRUE);		/* We get F1, F2 etc..		*/
	noecho();			/* Don't echo() while we do getch */

    int n = 0;
    int score = 0;
    char dir[6];

    mvprintw(0,0,"Player %c score: %d", ch, score);
    mvprintw(2,0,"Press Q do disconnect.");
    refresh();

    int key;
    do
    {
    	key = getch();		
        n++;
        switch (key)
        {
        case KEY_LEFT:
            move(1,0);
            clrtoeol();
            mvprintw(1,0,"%d: Left arrow is pressed", n);
            strcpy(dir, "LEFT");
            break;
        case KEY_RIGHT:
            move(1,0);
            clrtoeol();
            mvprintw(1,0,"%d: Right arrow is pressed", n);
            strcpy(dir, "RIGHT");
            break;
        case KEY_DOWN:
            move(1,0);
            clrtoeol();
            mvprintw(1,0,"%d: Down arrow is pressed", n);
            strcpy(dir, "DOWN");
            break;
        case KEY_UP:
            move(1,0);
            clrtoeol();
            mvprintw(1,0,"%d: Up arrow is pressed", n);
            strcpy(dir, "UP");
            break;
        case 'q':
        case 'Q':
            key = 'q';
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
            char character[2];
            character[0] = ch;
            character[1] = '\0';
            nbytes = s_sendmore(requester, character);
            assert(nbytes != -1);
            nbytes = s_sendmore(requester, dir);
            assert(nbytes != -1);
            nbytes = send_integer(requester, nonce, 0);
            assert(nbytes != -1);

            // response: success or fail + score + new nonce
            char *response = s_recv(requester);
            assert(response != NULL);

            if (response[0] == RESPONSE_FAIL) {
                free(response);
                continue;
            }

            int new_score = receive_integer(requester);
            assert(new_score != -1);
            score = new_score;

            int new_nonce = receive_integer(requester);
            assert(new_nonce != -1);
            nonce = new_nonce;

            free(response);

            move(0,0);
            clrtoeol();
            mvprintw(0,0,"Player %c score: %d", ch, score);
        }
        refresh();			/* Print it on to the real screen */
    } while (key != 'q' && key != 'Q');
    
    // send disconnect message to server
    // disconnect message: message type + char of lizard + nonce
    message_type[0] = DISCONNECT;
    nbytes = s_sendmore(requester, message_type);
    assert(nbytes != -1);
    char character[2];
    character[0] = ch;
    character[1] = '\0';
    nbytes = s_sendmore(requester, character);
    assert(nbytes != -1);
    nbytes = send_integer(requester, nonce, 0);
    assert(nbytes != -1);

    // response: success or fail
    char *response = s_recv(requester);
    assert(response != NULL);

    free(response);

    zmq_close (requester);
    zmq_ctx_destroy (context);
  	endwin();			/* End curses mode */

	return 0;
}