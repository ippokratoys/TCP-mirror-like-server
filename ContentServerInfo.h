#ifndef __CONTENT_SERVER_INFO
#define __CONTENT_SERVER_INFO
#include <string.h>

#define OK 0
#define NOT_FOUND 1
#define DIR_NOT_FOUND 2
typedef struct ContentServer {
    struct sockaddr_in  servadd; /* The address of server */
    char* name_of_server;
    int port;/*the ports that listens to*/
    char* dirorfile;/*the dir to fetch*/
    int delay;
}ContentServer;


#endif /* end of include guard: __CONTENT_SERVER_INFO */
