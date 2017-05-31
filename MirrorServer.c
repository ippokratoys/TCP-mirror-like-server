#include <fcntl.h>                 /*open the file to write*/
#include <stdio.h>
#include <stdlib.h>                               /* exit */
#include <string.h>                             /* strlen */
#include <unistd.h>                      /* STDOUT_FILENO */
#include <pthread.h>                       /* For threads */
#include <sys/stat.h>                      /*for dir check*/
#include <sys/types.h>                         /* sockets */
#include <sys/socket.h>                        /* sockets */
#include <netinet/in.h>               /* internet sockets */
#include <netdb.h>                       /* gethostbyname */
#include <arpa/inet.h>
#include <errno.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>

#include "ContentServerInfo.h"

int read_bytes(int fd,void *buff,int size){
    int all_read,now_read=0;
    for(all_read=0;all_read<size;all_read+=now_read){
        // printf("%d\n",now_read);
        if( (now_read=read(fd, buff + all_read, size-all_read)) ==-1){
            perror("In read_bytes");
            break;
        }
    }
    // printf("DOne\n");
    return all_read;
}



void perror_exit(char *msg);
void problem_arguments(char* my_msg){
    if(my_msg!=NULL){
        printf("%s\n",my_msg);
    }
    printf("correct usage of MirrorServer:\n");
    printf("    ./MirrorServer -m <buffer_dir> -p <MirrorServerPort> -w <NumberOfThreands>\n");
    printf("\t buffer_dir      : The folder to write every nessesary file\n");
    printf("\t MirrorServerPort: The port that the server listens to\n");
    printf("\t NumberOfThreands: Thread that the main thread creates \n");
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
/* Buffer bariables and mutexs*/
int number_of_active_manager=-1;
int number_of_worker_thread=-1;
int num_files_fetched=0;
int num_bytes_fetched=0;
int worker_in=0;
int manager_in=0;
int first_empty=0;
#define BUFF_SIZE 100
typedef struct BufferElement {
    char* a_file;//the full path of the file (or not)
    struct sockaddr_in* manager_info;//the info in order to open a socket
    int delay;//the delay
    int id;
}BufferElement;
typedef  struct {
    BufferElement communication_buffer[BUFF_SIZE];
    int start;
    int end;
    int count;
} FilesBuffer;
static pthread_mutex_t buffer_lock;
pthread_cond_t cond_finished;
pthread_cond_t not_full;
pthread_cond_t not_empty;
FilesBuffer the_buffer;

int fetch(BufferElement* file_info,char *folder_to_save){
    /*get the path of the new file*/
    int offset=0;
    char path_file[2048];
    strcpy(path_file, folder_to_save);
    int strlen_dir=strlen(path_file);
    path_file[strlen_dir]='/';
    strcpy(path_file+strlen_dir+1, file_info->a_file);
    int i;
    for (i = strlen_dir+1; i < strlen(path_file); i++) {
        if(path_file[i]=='/' ){
            path_file[i]='?';
        }
    }
    printf("The full file:%s\n\n",path_file);
    int my_copy_file=open(&path_file[offset],O_CREAT | O_TRUNC | O_WRONLY,0666);
    if(my_copy_file<0){
        fprintf(stderr, "%s ", &path_file[offset]);
        perror("Can't open");
        return 3;
    }
    printf("FILE OPENED %s\n", &path_file[offset]);
//    return 0;
    /*get a socket*/
    int file_transfer_socket;
    file_transfer_socket=socket(PF_INET, SOCK_STREAM, 0);
    if (file_transfer_socket<0){
        perror("socket in fetch");
        close(my_copy_file);
        return 1;
    }
    /*connect to the address*/
    if( connect(file_transfer_socket,(struct sockaddr*) file_info->manager_info, sizeof(*file_info->manager_info))!=0 ){
        perror("Connecting fetch");
        close(my_copy_file);
        close(file_transfer_socket);
        return 2;
    }
    write_bytes(file_transfer_socket, "FETCH", 6);
    int size_of_file=strlen(file_info->a_file)+1;
    //lenfth of directory
    write_bytes(file_transfer_socket, &size_of_file , sizeof(int));
    //the directory
    write_bytes(file_transfer_socket, file_info->a_file, size_of_file);
    //send the token
    ConnectionId the_token;
    the_token.delay=file_info->delay;
    the_token.id=file_info->id;
    write_bytes(file_transfer_socket, &the_token, sizeof(ConnectionId));
    int bytes_read_now = 0;
    int total_bytes_read = 0;
    char buffer_sock[1024];
    printf("start reading/writting\n");
    while( (bytes_read_now=read(file_transfer_socket,buffer_sock, 1024))>0 ){
//        printf("Bytes:%d\n", bytes_read_now);
        total_bytes_read+=bytes_read_now;
        write_bytes(my_copy_file, buffer_sock, bytes_read_now);
    }
    printf("END FETCHING size:%d %s\n\n",total_bytes_read,path_file);
    num_files_fetched++;
    num_bytes_fetched+=total_bytes_read;
    close(my_copy_file);
    close(file_transfer_socket);
    return 0;
}

void *worker_thread(void* arg){//takes as argument the dir where to save the files
    printf("Workder created\n");
    if(arg==NULL){
        pthread_exit(NULL);
    }
    BufferElement cur_buffer_elem;
    while(1){
        pthread_mutex_lock(&buffer_lock);
        while(the_buffer.count<=0 || worker_in>0 || manager_in>0){
            if(number_of_active_manager<=0 && the_buffer.count<=0){
                //no workers running and buffer empty
                pthread_cond_broadcast(&not_empty);
                pthread_mutex_unlock(&buffer_lock);
                printf("I guess this the end %ld\n",pthread_self());
                number_of_worker_thread--;
                pthread_cond_signal(&cond_finished);
                pthread_exit(NULL);
                //must unlock the mutex
            }
            pthread_cond_wait(&not_empty, &buffer_lock);
        }
        worker_in++;
        pthread_mutex_unlock(&buffer_lock);
        //do the reading
        memcpy(&cur_buffer_elem,&the_buffer.communication_buffer[the_buffer.start],sizeof(BufferElement));
        printf("(%d)(%d)I supose to fetch %s\n",number_of_active_manager,the_buffer.count,cur_buffer_elem.a_file);

        pthread_mutex_lock(&buffer_lock);
        worker_in--;
        the_buffer.count--;
        the_buffer.start = (the_buffer.start+1)%BUFF_SIZE;
        pthread_cond_signal(&not_full);
        pthread_mutex_unlock(&buffer_lock);

        //do the fetch of the file from cur_buffer_elem
        int return_of_fetch=fetch(&cur_buffer_elem,arg);

        free(cur_buffer_elem.a_file);
        free(cur_buffer_elem.manager_info);
        //free the element AND THE POINTERS
    }
    printf("Worker thread finished\n");
}


int init_conditions(int number_of_manager){
    the_buffer.start=0;
    the_buffer.end=-1;
    the_buffer.count=0;
    number_of_active_manager=number_of_manager;
    pthread_cond_init(&cond_finished, 0);
    pthread_cond_init(&not_full, 0);
    pthread_cond_init(&not_empty, 0);
}

int init_workers(int num_of_workers,char* dir_to_save){

    int i;
    number_of_worker_thread=num_of_workers;
    for (i = 0; i < num_of_workers; i++) {
        //ceate a worker thread and detach it
        pthread_t thr_p;
        if(pthread_create(&thr_p, NULL, (void*) worker_thread, dir_to_save)!=0){
            perror("creating thread");
        }
        pthread_detach(thr_p);
    }
}
void *mirror_manager_thread(void* arg){
    ContentServer* my_infos=(ContentServer *) arg;
    int j;
    int return_result;
    int my_content_server_sock;
    struct hostent* hosthp;
    struct sockaddr_in*  servadd=malloc(sizeof(struct sockaddr_in)); /* The address of server */
    hosthp=gethostbyname(my_infos->name_of_server);
    if(hosthp==NULL){
        return_result=NOT_FOUND;
        printf("ip NOT ok |%s|\t",my_infos->name_of_server);
        perror("get by name");//MUST CHANGE TTHIS
        pthread_exit(NULL);
        // write_bytes(initiator_fd,&return_result,sizeof(int));
        //and exit the thread returning not found
    }
    return_result=OK;
    servadd->sin_family=AF_INET;
    servadd->sin_port=htons(my_infos->port);
    printf("%s\n",inet_ntoa(*( (struct in_addr**) hosthp->h_addr_list)[0] ) );
    memcpy(&servadd->sin_addr, hosthp->h_addr_list[0], hosthp->h_length);
    //create a socket
    if(( my_content_server_sock = socket(PF_INET, SOCK_STREAM, 0) ) == -1){
        fprintf(stderr, "[%s]\t",my_infos->dirorfile);
        perror("my_content_server_sock");
    }
    //connect to the content server
    printf("Trying to connect\n");
    printf("PORT:%d Socket:%d\n",my_infos->port,my_content_server_sock );
    if( connect( my_content_server_sock, (struct sockaddr*) servadd, sizeof(*servadd) ) ==-1 ){
        fprintf(stderr, "[%s]\t",my_infos->dirorfile);
        perror("connect thread");exit(-1);
    }
    printf("DONE!\n");
    //write the command name
    write_bytes(my_content_server_sock, "LIST ", 6);
    // //write the length of the dir name element
    // int dir_name_size=strlen(my_infos->dirorfile)+1;
    // write_bytes(my_content_server_sock, &dir_name_size, sizeof(int));
    // //then write the dir name in did
    // write_bytes(my_content_server_sock, my_infos->dirorfile, dir_name_size);//write the directory name
    //
    // write_bytes(my_content_server_sock, &my_infos->delay, sizeof(int));//write the delay
    ConnectionId connection_token;
    connection_token.id=my_infos->id;
    connection_token.delay=my_infos->delay;
    write_bytes(my_content_server_sock,&connection_token,sizeof(ConnectionId));

    //get the reply from the Content Server
    char buffer_str[1024];
    FILE* remote_ls_fp=fdopen(my_content_server_sock,"r+");
    if(remote_ls_fp==NULL){
        perror("fdopen");
        pthread_exit(NULL);
    }
    int num_of_files=0;
    while(1){
        int bytes_read;
        int next_element_size=0;//read the size of the string
        if(fgets(buffer_str, 1023, remote_ls_fp)==NULL){
            //end of input
            if(num_of_files==0){
                //no files found or folder don't exists
                printf("Nothing found\n");
            }
            break;
        }
        if(buffer_str[0]=='\0')continue;
        if(strstr(buffer_str, my_infos->dirorfile)==NULL)continue;//if the file is a subfolder of what we need
        num_of_files++;
        //buffer_str ends with \n\0
        buffer_str[strlen(buffer_str)-1]='\0';
        // printf("|%s|\n", buffer_str);


        //get the lock
        pthread_mutex_lock(&buffer_lock);
        while(the_buffer.count>=BUFF_SIZE || manager_in>0 || worker_in>0){
            pthread_cond_wait(&not_full, &buffer_lock);
        }
        manager_in++;
        pthread_mutex_unlock(&buffer_lock);

        the_buffer.end=(the_buffer.end+1)%BUFF_SIZE;
        //save the dir name
        int offset=0;
        // if (strcmp(buffer_str, "./")==0) {
        //     printf("YEAP\n");
        //     offset=2;//cuts if needed the frist ./
        // }
        the_buffer.communication_buffer[the_buffer.end].a_file=malloc(strlen(buffer_str)+1);
        the_buffer.communication_buffer[the_buffer.end].id=my_infos->id;
        the_buffer.communication_buffer[the_buffer.end].delay=my_infos->delay;
        strcpy(the_buffer.communication_buffer[the_buffer.end].a_file,&buffer_str[offset]);
        //save the network address
        the_buffer.communication_buffer[the_buffer.end].manager_info=malloc(sizeof(*servadd));
        memcpy(the_buffer.communication_buffer[the_buffer.end].manager_info,servadd,sizeof(*servadd));
        printf("Wrote(manager) %s\n", the_buffer.communication_buffer[the_buffer.end].a_file );
        the_buffer.count++;


        pthread_mutex_lock(&buffer_lock);
        manager_in--;
        pthread_cond_signal(&not_empty);
        pthread_mutex_unlock(&buffer_lock);
        //free the lock
    }
    number_of_active_manager--;
    pthread_cond_signal(&not_empty);//so if someone waits to place somting wakes up to see that this is the end
    printf("!!END MIRRO manager  %ld \n",pthread_self());
    fclose(remote_ls_fp);//close the socket
    pthread_exit(NULL);
//    write_bytes(initiator_fd,&return_result,sizeof(int));
}



/* Write() repeatedly until 'size' bytes are written */
int write_all(int fd, void *buff, size_t size) {
    int sent, n;
    for(sent = 0; sent < size; sent+=n) {
        if ((n = write(fd, buff+sent, size-sent)) == -1)
            return -1; /* error */
    }
    return sent;
}

int isDirectory(const char *path) {
    struct stat statbuf;
    if (stat(path, &statbuf) != 0){
        return 0;
    }
    return S_ISDIR(statbuf.st_mode);
}
void print_statics(){
    printf("Files recieved : %d\n", num_files_fetched);
    printf("Bytes recieved : %d\n", num_bytes_fetched);
}
int main(int argc, char *argv[]) {
    int i;
    char* buff_dirname=NULL;
    int num_of_threads=-1,port=-1;
    for(i=1;i<argc;i++){
        if(strcmp(argv[i], "-p")==0){
            i++;if(argc==i)break;//if next arguemnt doesn't exitst
            port=atoi(argv[i]);
        }else if(strcmp(argv[i],"-m")==0){
            i++;if(argc==i)break;//if next arguemnt doesn't exitst
            buff_dirname=argv[i];
            struct stat statbuf;
            if( stat(buff_dirname, &statbuf)!=0 || ! S_ISDIR(statbuf.st_mode) ){
                fprintf(stderr, "This is not a directory [%s]\n", buff_dirname);
                problem_arguments(NULL);
                exit(-1);
            }

        }else if(strcmp(argv[i],"-w")==0){
            i++;if(argc==i)break;//if next arguemnt doesn't exitst
            num_of_threads=atoi(argv[i]);
        }else{
            problem_arguments(argv[i]);
        }
    }
    printf("Port:%d \n",port);
    printf("Directory:%s\n",buff_dirname);
    printf("Threads :%d\n", num_of_threads);
    struct sockaddr_in myaddr;  /* build our address here */
    int	c, lsock, csock;  /* listening and client sockets */
    FILE	*sock_fp;             /* stream for socket IO */
    FILE	*pipe_fp;	           /* use popen to run ls */
    char    dirname[BUFSIZ];               /* from client */
    char    command[BUFSIZ];               /* for popen() */

    /** create a TCP socket **/
    if ( (lsock = socket( PF_INET, SOCK_STREAM, 0)) < 0){
    	perror_exit( "socket" );
    }
    /** bind address to socket. **/
    myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    myaddr.sin_port = htons(port);/*port to bind socket*/
    myaddr.sin_family = AF_INET;  /* internet addr family */
    if( bind(lsock,(struct sockaddr *)&myaddr, sizeof(myaddr)) < 0){
        perror_exit( "bind" );
    }
    printf("sock:%d\n",lsock );
    /** listen for connections with Qsize=5 **/
    if ( listen(lsock, 13) != 0 ){
        perror_exit( "listen" );
    }
    /* accept connection, ignore client address */
   /* open socket as buffered stream */
   // if ((sock_fp = fdopen(csock,"r+")) == NULL){
   //     perror_exit("fdopen");
   // }
   int initiator_fd=-1;
   // printf("Accepting ... ");fflush(stdout);
   if( (initiator_fd=accept(lsock, NULL, NULL))==-1 ){
       perror_exit("accept problem");
   }
   // printf("OK %d\n",initiator_fd);
   int number_of_elements=0;
   ContentServer* my_content_servers=NULL;
   /*read number of elements*/
   read_bytes(initiator_fd,&number_of_elements,sizeof(int));
   // read(initiator_fd, &number_of_elements, sizeof(int));
   printf("Num elements:%d\n",number_of_elements);
   my_content_servers=malloc(sizeof(ContentServer)*number_of_elements);
   int j;

   init_conditions(number_of_elements);
   init_workers(num_of_threads,buff_dirname);
   for (j = 0; j < number_of_elements; j++) {
       printf("Read one\n");
//get the delay and the port
       read_bytes(initiator_fd, &my_content_servers[j], sizeof(ContentServer));
       my_content_servers[j].dirorfile=NULL;
       my_content_servers[j].name_of_server=NULL;
       my_content_servers[j].id=j;
//get the strings(first size then the string)
//directory/file name
       int size_of_dir_name=0,size_of_host_name=0;
       read_bytes(initiator_fd, &size_of_dir_name, sizeof(int));
       my_content_servers[j].dirorfile=malloc(size_of_dir_name);
       read_bytes(initiator_fd, my_content_servers[j].dirorfile, size_of_dir_name);
//host address
       read_bytes(initiator_fd, &size_of_host_name, sizeof(int));
       my_content_servers[j].name_of_server=malloc(size_of_host_name);
       read_bytes(initiator_fd, my_content_servers[j].name_of_server, size_of_host_name);
       //getchar();

       my_content_servers[j].servadd.sin_port=my_content_servers[j].port;
       struct hostent *hosthp;
       hosthp=gethostbyname(my_content_servers[j].name_of_server);
       int return_result;
       printf("next\n");
   }
   for (i = 0; i < number_of_elements; i++) {
       printf("Adress:%s\t", my_content_servers[i].name_of_server );
       printf("\tPort:%d \t Dir:%s \t Delay:%d\n",my_content_servers[i].port,my_content_servers[i].dirorfile,my_content_servers[i].delay );
       pthread_t thr_p;
       if(pthread_create(&thr_p, NULL, (void*) mirror_manager_thread, &my_content_servers[i])!=0){
           perror("creating thread");
       }
       printf("Thread %ld created\n", thr_p);
       pthread_detach(thr_p);//detach the new thread
   }
   pthread_mutex_lock(&buffer_lock);
   while(number_of_worker_thread>0){
       pthread_cond_wait(&cond_finished, &buffer_lock);
   }
   pthread_mutex_unlock(&buffer_lock);
   //do the accounting
   //release the buffer
   //destroy mutex and conditions
   // mirror_manager_thread(my_content_servers);
   printf("Main thread FINISHED\n");
   print_statics();
   pthread_exit(NULL);
}


void perror_exit(char *message) {
    perror(message);
    exit(EXIT_FAILURE);
}
