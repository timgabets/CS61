/**
 * CS61 Problem Set 6. Adversarial Network Pong.
 *
 * This assignment will teach you some useful and common strategies 
 * for handling problems common in networking, including loss, delay, 
 * and low utilization. It will also teach you programming using threads. 
 * The setting is a game called network pong.
 * 
 * Ricardo Contreras HUID 30857194 <ricardocontreras@g.harvard.edu>
 * Tim Gabets HUID 10924413 <tim@gabets.ru>
 * 
 * November-December 2013
 */

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <assert.h>
#include <pthread.h>
#include "serverinfo.h"

static const char* pong_host = PONG_HOST;
static const char* pong_port = PONG_PORT;
static const char* pong_user = PONG_USER;
static struct addrinfo* pong_addr;

typedef struct pong_args {
    int x;
    int y;
} pong_args;

pong_args pa;

pthread_mutex_t mutex;
pthread_mutex_t shutUpEverybody;
pthread_cond_t condvar;

// board dimensions
int width, height;
// initial ball position
int x = 0, y = 0;
// ball step size
int dx = 1, dy = 1;


// TIME HELPERS
double elapsed_base = 0;


/**
 * [timestamp description]
 * @return  [the current absolute time as a real number of seconds.]
 */
double timestamp(void) {
    struct timeval now;
    gettimeofday(&now, NULL);
    return now.tv_sec + (double) now.tv_usec / 1000000;
}


/**
 * [elapsed ]
 * @return  [the number of seconds that have elapsed since `elapsed_base`.]
 */
double elapsed(void) {
    return timestamp() - elapsed_base;
}


// http_connection
//    This object represents an open HTTP connection to a server.
typedef struct http_connection http_connection;
struct http_connection {
    int fd;                 // Socket file descriptor
    int state;              // Response parsing status (see below)
    int status_code;        // Response status code (e.g., 200, 402)
    size_t content_length;  // Content-Length value
    int has_content_length; // 1 iff Content-Length was provided
    int eof;                // 1 iff connection EOF has been reached
    char buf[BUFSIZ];       // Response buffer
    size_t len;             // Length of response buffer
    struct http_connection *next;
};

// head of the connection table
http_connection* head = NULL;

// `http_connection::state` constants
#define HTTP_REQUEST 0      // Request not sent yet
#define HTTP_INITIAL 1      // Before first line of response
#define HTTP_HEADERS 2      // After first line of response, in headers
#define HTTP_BODY    3      // In body
#define HTTP_DONE   (-1)    // Body complete, available for a new request
#define HTTP_CLOSED  (-2)   // Body /, connection closed
#define HTTP_BROKEN  (-3)   // Parse error

// helper functions
char* http_truncate_response(http_connection* conn);
static int http_process_response_headers(http_connection* conn);
static int http_check_response_body(http_connection* conn);

static void usage(void);

/**
 * [http_connect Opens a new connection to the server described by `ai`.]
 * @param  ai [address information data]
 * @return    [Returns a new `http_connection` object for that server 
 *             connection. Exits with an error message if the connection fails.]
 */
http_connection* http_connect(const struct addrinfo* ai) {
    // connect to the server
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        exit(1);
    }

    int yes = 1;
    (void) setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

    int r = connect(fd, ai->ai_addr, ai->ai_addrlen);
    if (r < 0) {
        perror("connect");
        exit(1);
    }

    // construct an http_connection object for this connection
    http_connection* conn =
        (http_connection*) malloc(sizeof(http_connection));
    conn->fd = fd;
    conn->state = HTTP_REQUEST;
    conn->eof = 0;
    conn->next = NULL;
    return conn;
}


/**
 * [http_close    Closes the HTTP connection `conn` and free its resources.]
 * @param conn [connection data pointer]
 */
void http_close(http_connection* conn) {
    close(conn->fd);
    free(conn);
}


/**
 * [http_send_request Sends an HTTP POST request for `uri` to connection `conn`.
 *                    Exit on error.]
 * @param conn [connection data pointer]
 * @param uri  [URI]
 */
void http_send_request(http_connection* conn, const char* uri) {
    assert(conn->state == HTTP_REQUEST || conn->state == HTTP_DONE);

    // prepare and write the request
    char reqbuf[BUFSIZ];
    size_t reqsz = sprintf(reqbuf,
                           "POST /%s/%s HTTP/1.0\r\n"
                           "Host: %s\r\n"
                           "Connection: keep-alive\r\n"
                           "\r\n",
                           pong_user, uri, pong_host);
    size_t pos = 0;
    while (pos < reqsz) {
        ssize_t nw = write(conn->fd, &reqbuf[pos], reqsz - pos);
        if (nw == 0)
            break;
        else if (nw == -1 && errno != EINTR && errno != EAGAIN) {
            perror("write");
            exit(1);
        } else if (nw != -1)
            pos += nw;
    }

    if (pos != reqsz) {
        fprintf(stderr, "%.3f sec: connection closed prematurely\n",
                elapsed());
        exit(1);
    }

    // clear response information
    conn->state = HTTP_INITIAL;
    conn->status_code = -1;
    conn->content_length = 0;
    conn->has_content_length = 0;
    conn->len = 0;
}


/**
 * [http_receive_response_headers Read the server's response headers. ]
 * @param conn [On return, `conn->status_code` holds the server's status code. 
 *              If the connection terminates prematurely, `conn->status_code` is -1.]
 */
void http_receive_response_headers(http_connection* conn) {
    assert(conn->state != HTTP_REQUEST);
    if (conn->state < 0)
        return;

    // read & parse data until told `http_process_response_headers`
    // tells us to stop
    while (http_process_response_headers(conn)) {

        if ( conn->len != 0) {
        conn->len = 0;
    } 
      
        ssize_t nr = read(conn->fd, &conn->buf[conn->len], BUFSIZ);
        if (nr == 0)
            conn->eof = 1;
        else if (nr == -1 && errno != EINTR && errno != EAGAIN) {
            perror("read");
            exit(1);
        } else if (nr != -1)
            conn->len += nr;
    }

    // Status codes >= 500 mean we are overloading the server
    // and should exit.
    if (conn->status_code >= 500) {
        fprintf(stderr, "%.3f sec: exiting because of "
                "server status %d (%s)\n", elapsed(),
                conn->status_code, http_truncate_response(conn));
        exit(1);
    }
}


/**
 * [http_receive_response_body Read the server's response body. On return, 
 *         `conn->buf` holds theresponse body, which is `conn->len` bytes 
 *         long and has been null-terminated.]
 * @param conn [connection data pointer]
 */
void http_receive_response_body(http_connection* conn) {
    assert(conn->state < 0 || conn->state == HTTP_BODY);
    if (conn->state < 0)
        return;

    // read response body (http_check_response_body tells us when to stop)
    while (http_check_response_body(conn)) {
        ssize_t nr = read(conn->fd, &conn->buf[conn->len], BUFSIZ);
        if (nr == 0)
            conn->eof = 1;
        else if (nr == -1 && errno != EINTR && errno != EAGAIN) {
            perror("read");
            exit(1);
        } else if (nr != -1)
            conn->len += nr;
    }

    // null-terminate body
    conn->buf[conn->len] = 0;
}


/**
 * [http_truncate_response Truncate the `conn` response text to a manageable 
 *                         length and return that truncated text. Useful for 
 *                         error messages.]
 * @param  conn [connection data pointer]
 * @return      [truncated string]
 */
char* http_truncate_response(http_connection* conn) {
    char *eol = strchr(conn->buf, '\n');
    if (eol)
        *eol = 0;
    if (strnlen(conn->buf, 100) >= 100)
        conn->buf[100] = 0;
    return conn->buf;
}

/**
 * [check_connection checks for existing connection]
 * @param  currentList [description]
 * @return             [connection data pointer]
 */
http_connection* check_connection(int currentList)
{
    http_connection* conn;

    if (head == NULL) {
        conn = http_connect(pong_addr);
        head = conn;
        return conn;
    } else {         
        
        http_connection* temp = head;
        http_connection* prev = NULL;
        // running through a linked list:
        while(temp != NULL)
        {
            switch(temp -> state)
            {
                case HTTP_DONE:
                case HTTP_REQUEST:
                    conn = temp;
                    return conn;

                case HTTP_BROKEN:
                {
                    http_connection* new = http_connect(pong_addr);
                    new -> next = temp -> next;

                    if(prev != NULL)
                        prev -> next = new;
                    else
                        head = new;
                    
                    http_close(temp);
                    return new;
                }
            }
        
            temp = temp -> next;    
        } // end while
    } 
/*        
      if (temp -> next == NULL) {
            conn = http_connect(pong_addr);
            temp -> next = conn;
            return conn;
        } else { 
         
        // Loop thorugh linked list 
        while(temp ->next != NULL) 
        {
            currentList ++;
            http_connection *nextConnTemp = temp->next;
            http_connection *oldConnTemp = temp;
            temp = nextConnTemp;
            
            switch(temp -> state)
            {
                case HTTP_DONE:
                case HTTP_REQUEST:
                    conn = temp;
                    return conn;

                case HTTP_BROKEN:
                {
                    http_connection *connTemp2 = http_connect(pong_addr);
                    connTemp2 -> next = temp -> next;
                    oldConnTemp -> next = connTemp2;
                    http_close(temp);
                    conn = connTemp2;
                    return conn;
                }

                default: 
                    // There was no connection. If we have less then 25 connections...
                    // Add a new conection to the linked list. If not start itereating again the linked list.
                    if (temp -> next == NULL) 
                    {
                        if (currentList > 25)
                        {
                            temp = head;
                            currentList = 0;
                        } else 
                        {
                            conn = http_connect(pong_addr);
                            temp -> next = conn;
                            return conn;
                        }
                    }                    
            }   // end switch
        }   // end while

    //}
    }
*/
    return NULL;

}

/**
 * [pong_thread Connect to the server at the position indicated by `threadarg`
 * (which is a pointer to a `pong_args` structure).]
 * @param unused [unused]
 */
void* pong_thread(void* unused) {
    pthread_detach(pthread_self());

    char url[256];
    http_connection* conn;
    int waitTime = 1,
        skip = 0,
        currentList = 0;

    pthread_mutex_lock(&shutUpEverybody);
    snprintf(url, sizeof(url),  "move?x=%d&y=%d&style=on",
             pa.x, pa.y);
    pthread_mutex_unlock(&shutUpEverybody);

    while (1) 
    {
        conn = check_connection(currentList);
   
        http_send_request(conn, url);
        http_receive_response_headers(conn);

        switch(conn->status_code)
        {
            case 200:
            {
                skip = 1;
                pthread_cond_signal(&condvar);
                http_receive_response_body(conn);
       
                int result = strncmp("0 OK", conn -> buf, 4);
                if( result != 0 )
                {
                    // STOP
                    pthread_mutex_lock(&shutUpEverybody);
        
                    // Parse the time to stop
                    char* waitTimeString = &(conn -> buf[1]);
                    int i = 0;
                    while(waitTimeString[i] != ' ')
                        i++;

                    waitTimeString[i] = '\0';
                    waitTime = atoi(waitTimeString);          
            
                    // Sleep for given time
                    usleep(waitTime * 1000);      
            
                    pthread_mutex_unlock(&shutUpEverybody);
                }
                
                break; // from swicth
            }

            case -1:
                // Retry...
                usleep(waitTime * 100000);
                waitTime *= 2 ;
                break;

            default:
                fprintf(stderr, "%.3f sec: warning: %d,%d: server returned status %d (expected 200)\n", elapsed(), pa.x, pa.y, conn->status_code);        
        }

        if(skip == 1)
            break; // from while
    }

    // signal the main thread to continue
    if (skip == 0) 
        pthread_cond_signal(&condvar);
    
    pthread_exit(NULL);
}


/**
 * [usage Explains how pong61 should be run.]
 */
static void usage(void) {
    fprintf(stderr, "Usage: ./pong61 [-h HOST] [-p PORT] [USER]\n");
    exit(1);
}


/**
 * [http_process_response_headers Parse the response represented by `conn->buf`. Returns 1
 * if more header data remains to be read, 0 if all headers have been consumed.] 
 * @param  conn [connection data pointer]
 * @return      [connection state]
 */
static int http_process_response_headers(http_connection* conn) {
    size_t i = 0;
    while ((conn->state == HTTP_INITIAL || conn->state == HTTP_HEADERS)
           && i + 2 <= conn->len) {
      if (conn->buf[i] == '\r' && conn->buf[i+1] == '\n') {
      conn->buf[i] = 0;
            if (conn->state == HTTP_INITIAL) {
            int minor;
                if (sscanf(conn->buf, "HTTP/1.%d %d",
                           &minor, &conn->status_code) == 2)
                    conn->state = HTTP_HEADERS;
                else
                    conn->state = HTTP_BROKEN;
            } else if (i == 0)
                conn->state = HTTP_BODY;
            else if (strncmp(conn->buf, "Content-Length: ", 16) == 0) {
                conn->content_length = strtoul(conn->buf + 16, NULL, 0);
                conn->has_content_length = 1;
            }
            memmove(conn->buf, conn->buf + i + 2, conn->len - (i + 2));
            conn->len -= i + 2;
            i = 0;
      } else
            ++i;
    }

    if (conn->eof)
        conn->state = HTTP_BROKEN;
    return conn->state == HTTP_INITIAL || conn->state == HTTP_HEADERS;
}


/**
 * [http_check_response_body checks and changes the state of the connection]
 * @param  conn [connection data pointer]
 * @return      [1 if more response data should be read into `conn->buf`,
 *               0 if the connection is broken or the response is complete.]
 */
static int http_check_response_body(http_connection* conn) {
  if (conn->state == HTTP_BODY
        && (conn->has_content_length || conn->eof)
        && conn->len >= conn->content_length) {
        conn->state = HTTP_DONE; 
    }
    if (conn->eof && conn->state == HTTP_DONE) {
        conn->state = HTTP_CLOSED; 
    }
    else if (conn->eof) {
        conn->state = HTTP_BROKEN;
    }
    return conn->state == HTTP_BODY;
    
}

/**
 * [init_board resets pong board and get its dimensions]
 * @param nocheck [description]
 */
void init_board(int nocheck)
{
    http_connection* conn = http_connect(pong_addr);
    http_send_request(conn, nocheck ? "reset?nocheck=1" : "reset");
    http_receive_response_headers(conn);
    http_receive_response_body(conn);
    if (conn->status_code != 200
        || sscanf(conn->buf, "%d %d\n", &width, &height) != 2
        || width <= 0 || height <= 0) {
        fprintf(stderr, "bad response to \"reset\" RPC: %d %s\n",
                conn->status_code, http_truncate_response(conn));
        exit(1);
    }
    http_close(conn);
}

/**
 * [update_position updates current position of the ball]
 */
void update_position(void)
{
    x += dx;
    y += dy;
    if (x < 0 || x >= width) {
        dx = -dx;
        x += 2 * dx;
    }
    if (y < 0 || y >= height) {
        dy = -dy;
        y += 2 * dy;
    }

    pa.x = x;
    pa.y = y;
}

/**
 * [main The main loop]
 * @param  argc [number of arguments]
 * @param  argv [arguments]
 * @return      [nothing]
 */
int main(int argc, char** argv) {
    // parse arguments
    int ch, nocheck = 0;
    while ((ch = getopt(argc, argv, "nh:p:u:")) != -1) {
        if (ch == 'h')
            pong_host = optarg;
        else if (ch == 'p')
            pong_port = optarg;
        else if (ch == 'u')
            pong_user = optarg;
        else if (ch == 'n')
            nocheck = 1;
        else
            usage();
    }
    if (optind == argc - 1)
        pong_user = argv[optind];
    else if (optind != argc)
        usage();

    // look up network address of pong server
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_NUMERICSERV;
    int r = getaddrinfo(pong_host, pong_port, &hints, &pong_addr);
    if (r != 0) {
        fprintf(stderr, "problem looking up %s: %s\n",
                pong_host, gai_strerror(r));
        exit(1);
    }

    // 
    init_board(nocheck);
    // measure future times relative to this moment
    elapsed_base = timestamp();

    // print display URL
    printf("Display: http://%s:%s/%s/%s\n",
           pong_host, pong_port, pong_user,
           nocheck ? " (NOCHECK mode)" : "");

    // initialize global synchronization objects
    pthread_mutex_init(&mutex, NULL);
    pthread_mutex_init(&shutUpEverybody, NULL);
    pthread_cond_init(&condvar, NULL);

    while (1)
    {
        // creating new thread to handle the next position
        pthread_t pt;
        if (pthread_create(&pt, NULL, pong_thread, NULL))
        {
            fprintf(stderr, "%.3f sec: pthread_create: %s\n",elapsed(), strerror(r));
            exit(1);
        }

        // wait until that thread signals us to continue
        pthread_mutex_lock(&mutex);
        pthread_cond_wait(&condvar, &mutex);
        pthread_mutex_unlock(&mutex);

        update_position();    

        // wait 0.1sec
        usleep(100000);
    }
}



