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
#include <time.h>

#define WINDOW_SIZE 30

int main()
{
    void *context = zmq_ctx_new ();
    // Connect to the server using ZMQ_SUB
    void *subscriber = zmq_socket (context, ZMQ_SUB);
    zmq_connect (subscriber, "tcp://localhost:5556");
    // subscribe to topics
    zmq_setsockopt (subscriber, ZMQ_SUBSCRIBE, "field_update", 8);

    initscr();		    	
	cbreak();				
    keypad(stdscr, TRUE);   
	noecho();

    /* creates a window and draws a border */
    WINDOW * my_win = newwin(WINDOW_SIZE, WINDOW_SIZE, 0, 0);
    box(my_win, 0 , 0);	
	wrefresh(my_win);

    char *character;
    char *pos_x;
    char *pos_y;
    char *subscription;

    while (1) {
        // receive char and position
        subscription = s_recv(subscriber);
        assert(subscription != NULL);
        character = s_recv(subscriber);
        assert(character != NULL);
        pos_x = s_recv(subscriber);
        assert(pos_x != NULL);
        pos_y = s_recv(subscriber);
        assert(pos_y != NULL);

        /* draw mark on new position */
        wmove(my_win, pos_x[0], pos_y[0]);
        waddch(my_win,character[0]| A_BOLD);
        wrefresh(my_win);
    }

    free(character);
    free(pos_x);
    free(pos_y);
    free(subscription);

    zmq_close (subscriber);
    zmq_ctx_destroy (context);
    endwin();			/* End curses mode */

    return 0;
}