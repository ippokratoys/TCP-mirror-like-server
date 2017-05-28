#include <fcntl.h>                 /*open the file to write*/
#include <stdio.h>
#include <stdlib.h>                               /* exit */
#include <string.h>                             /* strlen */
#include <unistd.h>                      /* STDOUT_FILENO */
#include <sys/types.h>                         /* sockets */
#include <sys/socket.h>                        /* sockets */
#include <netinet/in.h>               /* internet sockets */
#include <netdb.h>                       /* gethostbyname */
#include <arpa/inet.h>
#include <errno.h>

void perror_exit(char *message) {
    perror(message);
    exit(EXIT_FAILURE);
}

void problem_arguments(char* my_msg){
    if(my_msg!=NULL){
        printf("%s\n",my_msg);
    }
    printf("correct usage of ContentServer:\n");
    printf("    ./MirrorServer -p <ContentServerPort> -d <dirorfilename>\n");
    printf("\t dirorfilename      : The directory or file that's available for share to others\n");
    printf("\t ContentServerPort  : The port that the server listens to\n");
}


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
        // printf("%d\n",now_read);
        if( (now_read=read(fd, buff+all_read, size-all_read)) ==-1){
            perror("In read_bytes");
            break;
        }
    }
    // printf("DOne\n");
    return all_read;
}

int do_fetch(int server_fd){
    int size_of_str;
    read_bytes(server_fd, &size_of_str, sizeof(int));
    char* file_to_sent_str=malloc(sizeof(char*)*size_of_str);
    read_bytes(server_fd, file_to_sent_str, size_of_str);
    printf("I will send back : %s\n",file_to_sent_str);
    int file_to_sent_fd=open(file_to_sent_str,O_RDONLY);
    if(file_to_sent_fd<0){
        perror("Opening file");
    }
    printf("Opened OK\n");
    char filebuffer[1024];
    int num_of_bytes_read=0;
    int total_num_of_bytes=0;
    while ( (num_of_bytes_read=read(file_to_sent_fd, filebuffer, 1024))>0){
        total_num_of_bytes+=num_of_bytes_read;
        write_bytes(server_fd, filebuffer, num_of_bytes_read);
    }
    printf("Finished recievi\t SENT:%d\n",total_num_of_bytes);
    close(file_to_sent_fd);
    free(file_to_sent_str);
    return 0;
}

int do_list(int server_fd){
    char buffer[1024];
    int read_len;
    read_bytes(server_fd, &read_len, sizeof(int));
    //get the dir name
    read_bytes(server_fd, buffer, read_len);
    //get the delay
    int delay=0;
    read_bytes(server_fd, &delay, sizeof(int));
    printf("DIR:%s Delay:%d\n",buffer,delay);
    //create the ls
    char the_command[1024];
    sprintf(the_command, "find %s -type f",buffer);
    // printf("The find command:%s\n",the_command);
    //do the ls with popen
    FILE* the_list=popen(the_command,"r");

    if(fgets(buffer, 1024, the_list)==NULL){
        printf("NOOthing\n");
        //no results found
    }else if(memcmp(buffer, "find: ", 7)==0){
        printf("Error?\n");
        //that means folder not found
    }else{
        //somthing found
        while(fgets(buffer, 1024, the_list)!=NULL){
            // buffer[strlen(buffer)]='\n';//change the last from "\0" to "\n"
            if(buffer[0]=='\n' || buffer[0]=='\0')continue;
            printf("buffer:|%s|\n",buffer);
            write_bytes(server_fd, buffer,strlen(buffer));
        }
    }
    //finish
    read_len=0;
    write(server_fd, &read_len, sizeof(int));
    pclose(the_list);
    return 0;
}

int main(int argc, char *argv[]) {
    int i;
    char* content_dirname=NULL;
    int port=-1;
    for(i=1;i<argc;i++){
        if(strcmp(argv[i], "-p")==0){
            i++;if(argc==i)break;//if next arguemnt doesn't exitst
            port=atoi(argv[i]);
        }else if(strcmp(argv[i],"-d")==0){
            i++;if(argc==i)break;//if next arguemnt doesn't exitst
            content_dirname=argv[i];
        }else{
            problem_arguments(argv[i]);
            exit(-1);
        }
    }
    printf("Port:%d Direcory:%s\n",port,content_dirname );
    int l_sock;//the listen socket
    if((l_sock= socket(PF_INET, SOCK_STREAM, 0)) == -1){
        perror_exit("socket");
    }
    struct sockaddr_in in_all_addr;
    in_all_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    in_all_addr.sin_port=htons(port);
    in_all_addr.sin_family= AF_INET;
    //do the binding
    if(bind(l_sock, (struct sockaddr *) &in_all_addr, sizeof(in_all_addr))==-1){
        perror_exit("bind");
    }
    if(listen(l_sock, 10)==-1){
        perror_exit("listen");
    }
    //wait for connection
    socklen_t new_sock_len=sizeof(struct sockaddr_in);
    struct sockaddr_in* new_addr=NULL;
    char buffer[1024];int read_len=0;

    while(1){
        new_addr=malloc(sizeof(struct sockaddr_in));
        new_sock_len=sizeof(struct sockaddr_in);
        printf("Waiting for Adress\n");
        int cur_fd=accept(l_sock,(struct sockaddr *) new_addr, &new_sock_len);
        // int cur_fd=accept(l_sock,(struct sockaddr *) NULL, NULL);
        if(cur_fd<0){
            perror_exit("accepting");
        }
        printf("New connecton from(%d):%s\n",new_sock_len, inet_ntoa(new_addr->sin_addr));
        //read the command
        read_bytes(cur_fd,buffer , 6);
        if(strcmp(buffer, "LIST ")==0){
            do_list(cur_fd);
            close(cur_fd);
            //get the size of the dir
        }else if(strcmp(buffer, "FETCH")==0){
            do_fetch(cur_fd);
        }else{
            fprintf(stderr, "WHAT THE FUCK %s\n", buffer);
        }
        int dir_len;
        close(cur_fd);
        // printf("New thing :%s\n", inet_ntoa(new_addr->sin_addr.s_addr));
    }
    return 0;
}
