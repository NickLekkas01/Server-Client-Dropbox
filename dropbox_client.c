#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include <string.h>
#include <arpa/inet.h>
#include <inttypes.h>
#include <pthread.h>
#include <dirent.h>
#include <string.h>
#include "header.h"
#include <fcntl.h>

// #define POOL_SIZE 6

// #define PORT            5555
#define MESSAGE         "LOG_ON "
#define MESSAGE2        "GET_CLIENTS "
// #define SERVERHOST      "linux21.di.uoa.gr"
Client_data *start_client_list = NULL;


void init_sockaddr (struct sockaddr_in *name,
               const char *hostname,
               uint16_t port)
{
  struct hostent *hostinfo;

  name->sin_family = AF_INET;
  name->sin_port = htons (port);
  hostinfo = gethostbyname (hostname);
  if (hostinfo == NULL)
  {
    fprintf (stderr, "Unknown host %s.\n", hostname);
    exit (EXIT_FAILURE);
  }
  name->sin_addr = *(struct in_addr *) hostinfo->h_addr;
}


time_t get_version(char* path)
{
  struct stat stats;
  stat(path,&stats);

  return  stats.st_mtime;
}

 
/*function to get size of the file.*/
long int findSize(const char *file_name)
{
    struct stat st; /*declare stat variable*/
     
    /*get the size using stat()*/
     
    if(stat(file_name,&st)==0)
        return (st.st_size);
    else
        return -1;
}

void ask_file_list(int sock,struct in_addr* address,uint32_t port)
{
  char message[256];
  char path[128];
  int bytes;
  int files;
  time_t version;
  strcpy(message,"GET_FILE_LIST ");
  write(sock,message,strlen("GET_FILE_LIST ")+1);

  while((bytes = read(sock,message,strlen("FILE_LIST ") + 1 + sizeof(int)))<= 0){}
  memcpy(&files,message+strlen("FILE_LIST ") + 1,sizeof(int) );
  printf("%s: %d\n",message,files);

  while(files > 0 )
  {
    while(read(sock,message,128 +sizeof(uint64_t))<= 0){}
    memcpy(&version,message+128,sizeof(uint64_t));
    //printf("RECEIVER: %s - %ld\n",message, version); 
    place(&pool,address->s_addr,port,version,message);
    pthread_cond_signal(&cond_nonempty);
    
    files--;
  }
}

void ask_file(int sock,char *path,time_t version, struct in_addr* address,uint32_t port)
{
  char message[256];
  int bytes;
  int files;
  strcpy(message,"GET_FILE ");
  while ((write(sock,message,strlen("GET_FILE ")+1))==0);
  char temp_path[256];

  strcpy(temp_path, path);
  strcat(temp_path, " ");
  while((write(sock, temp_path, strlen(temp_path) + 1))==0);
  while((bytes = write(sock, &version, sizeof(time_t))) == 0);
  char buffer[256];
  char c[2];
  strcpy(buffer, "");
  while((bytes = read(sock, c, 1))==0);

  while(c[0] != ' ')
  {
      if (bytes < 0)
      {
          /* Read error. */
          perror ("read");
          exit (EXIT_FAILURE);
      }
      else if (bytes == 0)
      //     /* End-of-file. */
          return ;
      else
      {
          c[1]='\0';
          strcat(buffer, c);
          while((bytes = read(sock, c, 1))==0);
      }    

  }

  while((bytes = read(sock, c, 1))==0);
  if (bytes < 0)
  {
    perror ("read");
    exit (EXIT_FAILURE);
  }
  if(strcmp(buffer, "FILE_NOT_FOUND") == 0 || strcmp(buffer, "FILE_UP_TO_DATE") == 0)
  {
    printf("%s\n",buffer);
  }
  else if(strcmp(buffer, "FILE_SIZE") == 0)
  {
    time_t version2;
    while((bytes = read(sock, &version2, sizeof(time_t)) == 0)){}
    long int size;
    while((bytes = read(sock, &size, sizeof(long int)) == 0)){}
    //printf("%ld - %ld\n",version2, size);
    if(access(path, F_OK) != -1)
    {
      remove(path);
    }

    int fd = open(path, O_WRONLY | O_CREAT);
    for(long int i = 0; i < size; i++)
    {
      while((bytes = read(sock, &c[0], 1)) == 0){}
      while((bytes = write(fd, &c[0], 1)) == 0){}
    }
    close(fd);
  }
  

  // while((bytes = read(sock,message,strlen("FILE ") + 1 + sizeof(int)))<= 0){}
  // memcpy(&files,message+strlen("FILE_LIST ") + 1,sizeof(int) );
  // printf("%s: %d\n",message,files);

  // while(files > 0 )
  // {
  //   while(read(sock,message,128 +sizeof(uint64_t))<= 0){}
  //   memcpy(&version,message+128,sizeof(uint64_t));
  //   printf("RECEIVER: %s - %ld\n",message, version); 
  //   place(&pool,address->s_addr,port,version,message);
  //   pthread_cond_signal(&cond_nonempty);
    
  //   files--;
  // }
}

void* worker_Thread(void* ptr) 
{
  Pool_data *data;
  Client_data *temp = start_client_list;
  struct in_addr a;
  int sock;
  char ip[50];
  int client_sock;
  int reuse_addr = 1;
  setsockopt(client_sock, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr));
  while (1) 
  {
    data = obtain(&pool);
    pthread_cond_signal(&cond_nonfull);
    a.s_addr = data->IP;
    strcpy(ip, inet_ntoa(a));  

    if(data->path[0] == '\0')
    {
      printf("Asking File List From Client: %s %d\n", ip, data->port);
      
      client_sock = socket (PF_INET, SOCK_STREAM, 0);
      if (client_sock < 0)
      {
        perror ("socket (client)");
        exit (EXIT_FAILURE);
      }
      struct sockaddr_in clientname;
      /* Connect to the server. */
      init_sockaddr (&clientname, ip, data->port);
      if (0 > connect (client_sock,
                      (struct sockaddr *) &clientname,
                      sizeof (clientname)))
      {
        perror ("connect (client)");
        exit (EXIT_FAILURE);
      }
      ask_file_list(client_sock,&a,data->port);

    }
    else
    {
      ask_file(client_sock, data->path, data->version, &a,data->port);
    }
    

    usleep(500000);
  }
}

void write_to_server (int filedes, char *msg)
{
  int nbytes;
  
  nbytes = write (filedes, msg, strlen (msg) + 1);
  if (nbytes < 0)
  {
    perror ("write");
    exit (EXIT_FAILURE);
  }
  if(strcmp(msg, "GET_CLIENTS ") == 0 || strcmp(msg, "LOG_OFF ") == 0)
    return;
  struct hostent *hostinfo;
  char hostname[1024];
  hostname[1023] = '\0';
  gethostname(hostname, 1023);
  hostinfo = gethostbyname (hostname);
  if (hostinfo == NULL)
  {
    fprintf (stderr, "Unknown host %s.\n", hostname);
    exit (EXIT_FAILURE);
  }
  struct in_addr IP;
  char *IPbuffer;
  IPbuffer = inet_ntoa(*((struct in_addr*) 
                          hostinfo->h_addr_list[0])); 
  inet_aton( IPbuffer, &IP);
  uint32_t temp = ntohl(IP.s_addr);
  nbytes = write(filedes, &temp, sizeof(temp));
  if (nbytes < 0)
  {
    perror ("write");
    exit (EXIT_FAILURE);
  }
  
  
  temp = ntohl(client_port);
  nbytes = write(filedes, &temp, sizeof(temp));
  if (nbytes < 0)
  {
    perror ("write");
    exit (EXIT_FAILURE);
  }
}





void signal_arrived(int signal)
{
    write_to_server (sock,"LOG_OFF ");
    pthread_cond_destroy(&cond_nonempty);
    pthread_cond_destroy(&cond_nonfull);
    pthread_mutex_destroy(&pool_mtx);
    pthread_mutex_destroy(&list_mtx);
    close (sock);
    close(listeningSock);
    exit(0);
}

void handler (void)
{
  struct sigaction setup_action;
  sigset_t block_mask;

  sigemptyset (&block_mask);
  /* Block other terminal-generated signals while handler runs. */
  sigaddset (&block_mask, SIGINT);
  sigaddset (&block_mask, SIGUSR1);
  setup_action.sa_handler = signal_arrived;
  setup_action.sa_flags = 0;
  sigaction (SIGQUIT, &setup_action, NULL);
  
  sigemptyset (&block_mask);
  sigaddset (&block_mask, SIGQUIT);
  sigaddset (&block_mask, SIGUSR1);
  setup_action.sa_mask = block_mask;
  sigaction (SIGINT, &setup_action, NULL);
  
}

int read_from_server(int filedes, uint32_t myIP)
{
  char buffer[50];
  int nbytes;
  char c[2];
  strcpy(buffer, "");
  nbytes = read(filedes, c, 1);
  while(c[0] != ' ')
  {
      if (nbytes < 0)
      {
          /* Read error. */
          perror ("read");
          exit (EXIT_FAILURE);
      }
      else if (nbytes == 0)
      //     /* End-of-file. */
          return -1;
      else
      {
          c[1]='\0';
          strcat(buffer, c);
          nbytes = read(filedes, c, 1);
      }    
  }
  while((nbytes = read(filedes, c, 1))<=0);
  if (nbytes < 0)
  {
    perror ("read");
    exit (EXIT_FAILURE);
  }
  uint32_t IP, port;
  struct in_addr a, b;
  char myIPbuffer[20];
  b.s_addr = myIP;
  strcpy(myIPbuffer,inet_ntoa(b));
  if(strcmp(buffer, "CLIENT_LIST") == 0)
  {
    int N;
    while((nbytes = read(filedes, &N, sizeof(N)))<= 0 ){}
    if (nbytes < 0)
    {
      perror ("read");
      exit (EXIT_FAILURE);
    }
    for(int i = 0; i < N; i++)
    {
      while((nbytes = read(filedes, &IP, sizeof(IP)) <=0)){}
      if (nbytes < 0)
      {
          /* Read error. */
          perror ("read");
          exit (EXIT_FAILURE);
      }
      IP = htonl(IP);
      while((nbytes = read(filedes, &port, sizeof(port))<=0)){}
      if (nbytes < 0)
      {
          /* Read error. */
          perror ("read");
          exit (EXIT_FAILURE);
      }
      port = htonl(port);
      pthread_mutex_lock(&list_mtx);
      if(Node_Exists(start_client_list, IP, port, filedes) == 0)
      {
        a.s_addr = IP;
        if(strcmp(inet_ntoa(a), myIPbuffer) != 0 && client_port != port)
        {
          Insert_Node(&start_client_list, IP, port, filedes);
        }
      }
      pthread_mutex_unlock(&list_mtx);

    }
    printf("Client List\n");
  }
  else
  {
    while((nbytes = read(filedes, &IP, sizeof(IP))<=0));
    if (nbytes < 0)
    {
      perror ("read");
      exit (EXIT_FAILURE);
    }
    IP = htonl(IP);
    while((nbytes = read(filedes, &port, sizeof(port))<=0));
    if (nbytes < 0)
    {
      perror ("read");
      exit (EXIT_FAILURE);
    }
    port = htonl(port);
    
    a.s_addr = IP;
    if(strcmp(buffer,"USER_ON") == 0)
    {
      printf("%s - %s - %u\n",buffer, inet_ntoa(a), port);
      pthread_mutex_lock(&list_mtx);
      if(Node_Exists(start_client_list, IP, port, filedes) == 0)
      {
        Insert_Node(&start_client_list, IP, port, filedes);
        place(&pool, IP, port, 0, "\0");
        pthread_cond_signal(&cond_nonempty);
      }
      pthread_mutex_unlock(&list_mtx);
    }
    else if(strcmp(buffer,"USER_OFF") == 0)
    {
      printf("%s - %s - %u\n",buffer, inet_ntoa(a), port);
      pthread_mutex_lock(&list_mtx);
      if(Delete_Node(&start_client_list, IP, port) == 0)
      {
          perror("ERROR_IP_PORT_NOT_FOUND_IN_LIST");
          exit (EXIT_FAILURE);
      }
      pthread_mutex_unlock(&list_mtx);
    }
  }
  Print_List(start_client_list); 

  return 0;
}

int file_count(char* path)
{
    struct dirent* directory;
    DIR* dirptr;
    int counter = 0;
    dirptr = opendir(path);
    char filePath[128];

    while((directory = readdir(dirptr)) != NULL){
      if ( directory->d_name[0] == '.')// && directory->d_name[1] == '.')
        continue;

        memcpy(filePath,path,strlen(path));
        memcpy(filePath + strlen(path),"/", 1 );
        memcpy(filePath + strlen(path) + 1 , directory->d_name, strlen(directory->d_name) + 1);

        DIR* tempPtr;
        printf("%s\n", filePath);
        if(( tempPtr = opendir(filePath))!=NULL){
          closedir(tempPtr);
          counter += file_count(filePath);
          continue;
        }

        counter++;
    }

    closedir(dirptr);
    return counter;

}

void send_file_paths(int filedes, char* path){
  char filePath[128];
  char message[256];
  struct dirent* directory;
  DIR* dirptr;
  struct stat stats;
  

  dirptr = opendir(path);

  while((directory = readdir(dirptr))!= NULL){
    if(directory->d_name[0] == '.')// && directory->d_name[1] == '.')
      continue;

    sprintf(filePath,"%s/%s",path, directory->d_name);
    DIR *temp;
    if((temp = opendir(filePath)) != NULL){
      closedir(temp);
      send_file_paths(filedes,filePath);
      continue;
    }

    stat(filePath,&stats);

    memcpy(message,filePath,128);
    memcpy(message + 128,&stats.st_mtime,sizeof(uint64_t));

    write(filedes,message,128+sizeof(uint64_t));

  }

  closedir(dirptr);

}


int read_from_client (int filedes)
{
    char buffer[256];
    strcpy(buffer, "");
    int nbytes;
    char c[2];
    while((nbytes = read(filedes, c, 1))==0){}
    while(c[0] != ' ')
    {
        if (nbytes < 0)
        {
            /* Read error. */
            perror ("read");
            exit (EXIT_FAILURE);
        }
        else if (nbytes == 0)
            /* End-of-file. */
            return -1;
        else
        {
            c[1]='\0';
            strcat(buffer, c);
            while((nbytes = read(filedes, c, 1))==0);
        }    
    }
    while((nbytes = read(filedes, c, 1))==0){}
    if(strcmp(buffer, "GET_FILE_LIST") == 0)
    {
        int counter;
        char message[256];
        
        counter = file_count(dirName);

        sprintf(message,"FILE_LIST ");
        memcpy(message + strlen("FILE_LIST ") + 1,&counter,sizeof(int));
        write(filedes,message,strlen("FILE_LIST ") + 1 + sizeof(int));
        send_file_paths(filedes, dirName);

    }
    else if(strcmp(buffer, "GET_FILE") == 0)
    {
      char message[256];
      strcpy(buffer, "");
      while((nbytes = read(filedes, c, 1)==0));
      while(c[0] != ' ')
      {
          if (nbytes < 0)
          {
              /* Read error. */
              perror ("read");
              exit (EXIT_FAILURE);
          }
          else if (nbytes == 0)
              /* End-of-file. */
              return -1;
          else
          {
              c[1]='\0';
              strcat(buffer, c);
              while((nbytes = read(filedes, c, 1))==0);
          }    
      }
      while((nbytes = read(filedes, c, 1)==0));

      char path[256];
      strcpy(path, buffer);
      time_t version;
      while((nbytes = read(filedes,&version,sizeof(time_t)))<= 0){}
      if(access(path, F_OK) == -1)
      {
        strcpy(buffer, "FILE_NOT_FOUND ");
        write(filedes, buffer, strlen(buffer) + 1);
      }
      else
      {
        if(get_version(path) == version)
        {
          strcpy(buffer, "FILE_UP_TO_DATE ");
          write(filedes, buffer, strlen(buffer) + 1);          
        }
        else
        {
          sprintf(message,"FILE_SIZE ");
          write(filedes, message, strlen(message) + 1); 
          time_t version2 = get_version(path);
          // memcpy(message + strlen("FILE_SIZE ") + 1,&version2, sizeof(time_t));
          write(filedes, &version2, sizeof(time_t));
          long int size = findSize(path);
          // memcpy(message + strlen("FILE_SIZE ") + 1 + sizeof(time_t),&size,sizeof(long int));
          write(filedes, &size, sizeof(long int));
          int fd = open(path, O_RDONLY);
          strcpy(message, "");
          for(long int i = 0 ; i < size; i++)
          {
            while((nbytes = read(fd, &c[0], 1) == 0)){}
            //memcpy(message + i,&c[0],1);
            write(filedes, message + i, 1);

          }
          //write(filedes, message, strlen("FILE_SIZE ") + sizeof(time_t) + sizeof(long int) + size + 1);
          close(fd);
        }
        
      }
       
    }

}



int main (int argc, char *argv[])
{
  extern void init_sockaddr (struct sockaddr_in *name,
                             const char *hostname,
                             uint16_t port);
  struct sockaddr_in servername;
  fd_set active_fd_set, read_fd_set;
  size_t size;
  
  int workerThreads;
  char SERVERHOST[50];
  if(argc != 13)
  {
    printf("Wrong number of arguments\n");
    exit(-1);
  }
  for(int i = 1; i < 12; i+=2)
  {
    if(argv[i][1] == 'd')
      strcpy(dirName, argv[i+1]);
    if(argv[i][1] == 'p')
      client_port = atoi(argv[i+1]);
    if(argv[i][1] == 'w')
      workerThreads = atoi(argv[i+1]);
    if(argv[i][1] == 'b')
      POOL_SIZE = atoi(argv[i+1]);
    if(argv[i][1] == 's')
      if(argv[i][2] == 'p')
        PORT = atoi(argv[i+1]);
      else
      {
        if(argv[i][2] == 'i' && argv[i][3] == 'p')
          strcpy(SERVERHOST, argv[i+1]);
      }
  }
  pthread_t threads[10];

  handler();
  /* Create the socket. */
  sock = socket (PF_INET, SOCK_STREAM, 0);
  if (sock < 0)
  {
    perror ("socket (client)");
    exit (EXIT_FAILURE);
  }

  /* Connect to the server. */
  init_sockaddr (&servername, SERVERHOST, PORT);
  if (0 > connect (sock,
                   (struct sockaddr *) &servername,
                   sizeof (servername)))
  {
    perror ("connect (client)");
    exit (EXIT_FAILURE);
  }

  struct hostent *hostinfo;
  char hostname[1024];
  hostname[1023] = '\0';
  gethostname(hostname, 1023);
  hostinfo = gethostbyname (hostname);
  if (hostinfo == NULL)
  {
    fprintf (stderr, "Unknown host %s.\n", hostname);
    exit (EXIT_FAILURE);
  }
  struct in_addr IP;
  char *IPbuffer;
  IPbuffer = inet_ntoa(*((struct in_addr*) 
                          hostinfo->h_addr_list[0])); 
  inet_aton( IPbuffer, &IP);
  myIP = IP.s_addr;

  struct sockaddr_in listeningClient;
  struct sockaddr* listeningClientptr = (struct sockaddr*) &listeningClient;
  int listeningClientLen = sizeof(listeningClient);

  listeningSock = socket(AF_INET, SOCK_STREAM, 0);
  int reuse_addr = 1;
  setsockopt(listeningSock, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr));
  fcntl(listeningSock, F_SETFL, O_NONBLOCK);

  listeningClient.sin_family = AF_INET;
  listeningClient.sin_addr.s_addr = htonl(INADDR_ANY);
  listeningClient.sin_port = htons((in_port_t)client_port);

  if ( bind(listeningSock, listeningClientptr, listeningClientLen) != 0 ) 
  {
      printf("Bind Failed\n");
      exit(1);
  }

  listen(listeningSock, 10);

  /* Send data to the server. */
  char msg[20];
  strcpy(msg, "LOG_ON ");
  write_to_server (sock,msg);
  strcpy(msg, "GET_CLIENTS ");
  write_to_server (sock,msg);
  
  initialize(&pool);
  pthread_mutex_init(&list_mtx, 0);
  pthread_mutex_init(&pool_mtx, 0);
  pthread_cond_init(&cond_nonempty, 0);
  pthread_cond_init(&cond_nonfull, 0);
  
  read_from_server(sock, myIP);
  fill_pool(start_client_list);
  pthread_t cons, prod;

  
  for(int i = 0 ; i < workerThreads; i++)
    pthread_create(&threads[i], NULL, worker_Thread, NULL);
  // while(1)
  // {
  //   read_from_server(sock, myIP);
  // }



  

  /* Initialize the set of active sockets. */
  FD_ZERO (&active_fd_set);
  FD_SET (sock, &active_fd_set);
  FD_SET (listeningSock, &active_fd_set);

  while (1)
  {
    /* Block until input arrives on one or more active sockets. */
    read_fd_set = active_fd_set;
    if (select (FD_SETSIZE, &read_fd_set, NULL, NULL, NULL) < 0)
    {
    perror ("select");
    exit (EXIT_FAILURE);
    }

    /* Service all the sockets with input pending. */
    for (int i = 0; i < FD_SETSIZE; ++i)
    {
        if (FD_ISSET (i, &read_fd_set))
        {
            if (i == listeningSock)
            {
                /* Connection request on original socket. */
                int new;
                size = sizeof (listeningClient);
                new = accept (listeningSock,
                            (struct sockaddr *) &listeningClient,
                            (socklen_t *)&size);
                if (new < 0)
                {pthread_cond_signal(&cond_nonempty);
                 pthread_cond_signal(&cond_nonempty);
                    exit (EXIT_FAILURE);
                }
                fprintf (stderr,
                        "Server: connect from host %s, port %hd.\n",
                        inet_ntoa (listeningClient.sin_addr),
                        ntohs (listeningClient.sin_port));
                FD_SET (new, &active_fd_set);
            }
            else if(i == sock)
            {
                /* Data arriving on an already-connected socket. */
                if ( read_from_server (i, myIP) < 0)
                {
                    close (i);
                    FD_CLR (i, &active_fd_set);
                }

            }
            else
            {
              read_from_client(i);
            }
            
        }
    }
  }
  
  

  exit (EXIT_SUCCESS);
}


