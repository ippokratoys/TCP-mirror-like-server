#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "ContentServerInfo.h"
/*writes form the buff size bytes to the fd
returs bytes that had been written*/
int write_bytes(int fd, void *buff, int size) {
    int sent, n;
    for(sent = 0; sent < size; sent+=n) {
        if ((n = write(fd, buff+sent, size-sent)) == -1)
            return -1;
    }
    return sent;
}

int read_bytes(int fd,void *buff,int size){
    int all_read,now_read=0;
    for(all_read=0;all_read<size;all_read+=now_read){
        if( (now_read=read(fd, buff +all_read, size-all_read)) ==-1){
            perror("In read_bytes");
            break;
        }
    }
    return all_read;
}


void problem_arguments(char* my_msg){
    if(my_msg!=NULL){
        printf("%s\n",my_msg);
    }
    printf("correct usage of MirrorServer:\n");
    printf("    ./MirrorInitiator -n <MirrorServerAddress> -p <MirrorServerPort> \n-s <ContentServerAddress1:ContentServerPort1:dirorfile1:delay1, \nContentServerAddress2:ContentServerPort2:dirorfile2:delay2, ...>");
    printf("\t MirrorServerAddress   : It's the ip/name addres of the MirrorServer\n");
    printf("\t MirrorServerPort      : Port number where MirrorServer listens (int)\n");
    printf("\t ContentServerAddressX : It's the ip/name addres of a ContentServer \n");
    printf("\t ContentServerPortX    : Port number where ContentServer listens (int)\n");
    printf("\t dirorfileX            : File/Directory which must be Fetched (recursive)\n");
    printf("\t delayX                : Delay that will be used in this connection\n");
}


void perror_exit(char *message)
{
    perror(message);
    exit(EXIT_FAILURE);
}


ContentServer* get_content_servers(char* all_servers,int* num_of_servers){
    char* a_server_info;
    int written=0;int size=15;
    ContentServer* all_content_servers=malloc(sizeof(ContentServer)*size);
    ContentServer* cur_server=&all_content_servers[0];
    a_server_info=strtok(all_servers,":");

    while(a_server_info!=NULL){
        cur_server=&all_content_servers[written];

        a_server_info;//server addres
        struct hostent *hp;           /* to resolve server ip */
        cur_server->name_of_server=a_server_info;/*save it as a text*/
        // hp=gethostbyname(a_server_info);
        // memcpy(&cur_server->servadd.sin_addr , hp->h_addr , hp->h_length);

        a_server_info=strtok(NULL,":");
        cur_server->port=atoi(a_server_info);//serverport
        // cur_server->servadd.sin_port=htons(cur_server->port);
        // cur_server->servadd.sin_family=AF_INET;

        a_server_info=strtok(NULL,":");
        cur_server->dirorfile=a_server_info;//diroffile

        a_server_info=strtok(NULL,",");
        cur_server->delay=atoi(a_server_info);//delay

        a_server_info=strtok(NULL,":");

        written++;//

        if(written==size)
            if(realloc(all_content_servers, size=size+10))//if it's full, make new possitions
                perror("realoc");
    }
    if(num_of_servers!=NULL)*num_of_servers=written;
    return all_content_servers;
}

int main(int argc,char* argv[]){
    int i;
    char buffer[2048];
    ContentServer* my_content_servers;int num_content_servers=-1;
/*mirror server infos*/
    struct sockaddr_in  mirror_addr;
    struct hostent* mirror_host_net=NULL;
    int mirror_port=-1;

    for (i=1;i<argc;i++){
        if(strcmp(argv[i], "-n")==0){
            i++;//go to next argument
            if(i==argc)break;//if not exists break
            mirror_host_net=gethostbyname(argv[i]);//get the host by name
            if(mirror_host_net==NULL){
                perror("Problem wiht host by name");exit(-1);
            }
            if(mirror_host_net->h_addr_list[i]!=NULL){
                printf("addr list empty\n");
            }
            struct in_addr **my_addr_list=(struct in_addr **) mirror_host_net->h_addr_list;
            if(my_addr_list[0]==NULL)break;
            printf("%s\n", inet_ntoa(*my_addr_list[0]) );
            memcpy(&mirror_addr.sin_addr, mirror_host_net->h_addr_list[0], mirror_host_net->h_length);

        }else if(strcmp(argv[i], "-p")==0){
            i++;//go to next argument
            if(i==argc)break;//if not exists break
            mirror_port=atoi(argv[i]);
            if(mirror_port<=0 || mirror_port>67000){
                printf("Problem with port number\n");
                exit(-2);
            }
        }else if(strcmp(argv[i], "-s")==0){
            i++;
            if(i==argc)break;
            my_content_servers=get_content_servers(argv[i],&num_content_servers);
        }else{
            problem_arguments(argv[i]);
        }
    }
    // printf("%d\n", mirror_host_net->h_addrtype==AF_INET);
    printf("Content Servers:%d\n",num_content_servers);
    for (size_t i = 0; i < num_content_servers; i++) {
        printf("Add:%s\tPort:%d \t Dir:%s \t Delay:%d\n",my_content_servers[i].name_of_server,my_content_servers[i].port,my_content_servers[i].dirorfile,my_content_servers[i].delay );
    }
    printf("Ip:%s\n",inet_ntoa( *( (struct in_addr**) mirror_host_net->h_addr_list)[0] ));
    printf("Port:%d\n",mirror_port);



    struct sockaddr_in  servadd; /* The address of server */
    struct hostent *hp;           /* to resolve server ip */
    int    sock, n_read;     /* socket and message length */
    // char   buffer[1024];        /* to receive message */

    /*Get a socket */
    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1 ){
        perror_exit( "socket" );
    }
    /* Step 2: lookup server's address and connect there */
    memcpy(&servadd.sin_addr, mirror_host_net->h_addr, mirror_host_net->h_length);
    servadd.sin_port = htons(mirror_port); /* set port number */
    servadd.sin_family = AF_INET ;     /* set socket type */
    if ( connect(sock, (struct sockaddr*) &servadd, sizeof(servadd))!=0 ){
        perror_exit( "connect" );
    }
    printf("Conected to Mirror Server\n");
    /* Step 3: send directory name + newline */
    //tels him how many objects he will send
    write_bytes(sock, &num_content_servers, sizeof(int));
    for (size_t i = 0; i < num_content_servers; i++) {
        printf("Sent one\n");
        //write the details of the server(whole struct)
        write_bytes(sock, &my_content_servers[i], sizeof(ContentServer));
        //write the size of the directory
        int dir_name_len=strlen(my_content_servers[i].dirorfile)+1;
        write_bytes(sock, &dir_name_len, sizeof(int));
        //write the directory
        write_bytes(sock, my_content_servers[i].dirorfile,dir_name_len);

        int host_name_len=strlen(my_content_servers[i].name_of_server)+1;
        write_bytes(sock, &host_name_len, sizeof(int));
        write_bytes(sock, my_content_servers[i].name_of_server,host_name_len);

        // int status_send;
        // read_bytes(sock, &status_send, sizeof(int));
        // printf("READ : %d\n", status_send);
    }
    // write_bytes(sock, "my_content_servers", int size)
    /* Step 4: read back results and send them to stdout */

    close(sock);
    return 0;
}
