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


// #define POOL_SIZE 6

// #define PORT            5555
#define MESSAGE         "LOG_ON "
#define MESSAGE2        "GET_CLIENTS "
// #define SERVERHOST      "linux21.di.uoa.gr"

uint32_t myIP;
uint32_t client_port;
int sock;

int POOL_SIZE;
int PORT;
char dirName[40];

Client_data *start = NULL;

typedef struct pool_data
{
  uint32_t IP;
  uint32_t port;
  char path[128];
  time_t version;
}Pool_data;

typedef struct 
{
  Pool_data *data;
  int start;
  int end;
  int count;
} pool_t;


pthread_mutex_t mtx;
pthread_cond_t cond_nonempty;
pthread_cond_t cond_nonfull;
pool_t pool;

time_t get_version(char* path)
{
  struct stat stats;
  stat(path,&stats);

  return  stats.st_mtime;
}

void initialize(pool_t * pool) 
{
  pool->data = malloc(POOL_SIZE * sizeof(Pool_data));
  pool->start = 0;
  pool->end = -1;
  pool->count = 0;
}

void place(pool_t * pool, uint32_t IP, uint32_t port, time_t version, char *path) 
{
  pthread_mutex_lock(&mtx);
  while (pool->count >= POOL_SIZE) 
  {
    //printf(">> Found Buffer Full \n");
    pthread_cond_wait(&cond_nonfull, &mtx);
  }
  pool->end = (pool->end + 1) % POOL_SIZE;
  pool->data[pool->end].IP = IP;
  pool->data[pool->end].port = port;
  pool->data[pool->end].version =  version;
  strcpy(pool->data[pool->end].path, path);
  pool->count++;
  pthread_mutex_unlock(&mtx);
}

Pool_data *obtain(pool_t * pool) 
{
  Pool_data *data;
  data = malloc(sizeof(Pool_data));
  pthread_mutex_lock(&mtx);
  while (pool->count <= 0) 
  {
    printf(">> Found Buffer Empty \n");
    pthread_cond_wait(&cond_nonempty, &mtx);
  }
  data->IP = pool->data[pool->start].IP;
  data->port = pool->data[pool->start].port;
  strcpy(data->path, pool->data[pool->start].path);
  data->version = pool->data[pool->start].version;
  pool->start = (pool->start + 1) % POOL_SIZE;
  pool->count--;
  pthread_mutex_unlock(&mtx);
  return data;
}

void fill_pool() 
{
  Client_data *temp = start;
  char path[128];
  getcwd(path, sizeof(path));
  while (temp != NULL) 
  {
    place(&pool, temp->data->sin_addr.s_addr, temp->data->sin_port, 0, "\0");
    printf("producer: %s %u\n", inet_ntoa(temp->data->sin_addr), temp->data->sin_port);
    temp = temp->next;
    pthread_cond_signal(&cond_nonempty);
    usleep(300000);
  }
}

void ask_file_list(int sock,struct in_addr* address,uint32_t port){
  char message[256];
  char path[128];
  int bytes;
  int files;
  time_t version;
  strcpy(message,"GET_FILE_LIST ");
  write(sock,message,15);

  while((bytes = read(sock,message,11 + sizeof(int)))<= 0){}
  memcpy(&files,message+11,sizeof(int) );
  printf("%s: %d\n",message,files);

  while(files > 0 ){
    
    while(read(sock,message,128 +sizeof(uint64_t))<= 0){}
    
    memcpy(&version,message+128,sizeof(uint64_t));
    place(&pool,address->s_addr,port,version,message);
    pthread_cond_signal(&cond_nonempty);
    }
    files--;
  }
}


void* worker_Thread(void* ptr) 
{
  Pool_data *data;
  Client_data *temp = start;
  struct in_addr a;
  int sock;
  char ip[50];

  while (1) 
  {
    data = obtain(&pool);
    pthread_cond_signal(&cond_nonfull);
    a.s_addr = data->IP;
    strcpy(ip, inet_ntoa(a));  

    if(data->path[0] == '\0'){
    printf("Asking File List From Client: %s %d\n", ip, data->port);
    //FIX prepei na ginei to connection me ton client edo kai to socket pou tha epistrafei na dothei stin sinartisi apo kato

    ask_file_list(sock,&a,data->port);

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

int make_socket (uint16_t port)
{
    int sock;
    struct sockaddr_in name;

    /* Create the socket. */
    sock = socket (PF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror ("socket");
        exit (EXIT_FAILURE);
    }

    /* Give the socket a name. */
    name.sin_family = AF_INET;
    name.sin_port = htons (port);
    name.sin_addr.s_addr = htonl (INADDR_ANY);
    if (bind (sock, (struct sockaddr *) &name, sizeof (name)) < 0)
    {
      perror ("bind");
      exit (EXIT_FAILURE);
    }

  return sock;
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
      if(Node_Exists(start, IP, port, filedes) == 0)
      {
        a.s_addr = IP;
        if(strcmp(inet_ntoa(a), myIPbuffer) != 0 && client_port != port)
        {
          Insert_Node(&start, IP, port, filedes);
        }
      }
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
      if(Node_Exists(start, IP, port, filedes) == 0)
      {
        Insert_Node(&start, IP, port, filedes);
        place(&pool, IP, port, 0, "\0");
        pthread_cond_signal(&cond_nonempty);
      }
    }
    else if(strcmp(buffer,"USER_OFF") == 0)
    {
      printf("%s - %s - %u\n",buffer, inet_ntoa(a), port);
      if(Delete_Node(&start, IP, port) == 0)
      {
          perror("ERROR_IP_PORT_NOT_FOUND_IN_LIST");
          exit (EXIT_FAILURE);
      }
    }
  }
  Print_List(start); 

  return 0;
}

int read_from_client (int filedes, struct sockaddr_in clientname, int sock)
{
    char buffer[256];
    strcpy(buffer, "");
    int nbytes;
    char c[2];
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
            /* End-of-file. */
            return -1;
        else
        {
            c[1]='\0';
            strcat(buffer, c);
            nbytes = read(filedes, c, 1);
        }    
    }
    nbytes = read(filedes, c, 1);

    
    if(strcmp(buffer, "GET_FILE_LIST") == 0)
    {
        int counter;
        char message[256];
        
        counter = file_count(dirName);

        sprintf(message,"FILE_LIST ");
        memcpy(message + 11,&counter,sizeof(int));
        write(filedes,message,11 + sizeof(int));
    

    }

}

void send_file_paths(int filedes, char* path){
  char filePath[128];
  char message[256];
  struct dirent* directory;
  DIR* dirptr;
  struct stat stats;
  

  dirptr = opendir(path);

  while((directory = readdir(dirptr))!= NULL){
    if(directory->d_name[0] == '.' && directory->d_name == '.')
      continue;

    sprintf(filePath,"%s/%s",path,directory->d_name);
    
    if(opendir(filePath) != NULL){
      send_file_paths(filedes,path);
      continue;
    }

    stat(filePath,&stats);

    memcpy(message,filePath,128);
    memcpy(message + 128,&stats.st_mtime,sizeof(uint64_t));

    write(filedes,message,128+sizeof(uint64_t));

  }

  close(dirptr);

}

int file_count(char* path){
    struct dirent* directory;
    DIR* dirptr;
    int counter;
    dirptr = opendir(path);
    char filePath[128];

    while((directory = readdir(dirptr)) != NULL){
      if ( directory->d_name[0] == '.' && directory->d_name[1] == '.')
        continue;

        memcpy(filePath,path,strlen(path));
        memcpy(filePath + strlen(path),"/", 1 );
        memcpy(filePath + strelen(path) + 1 , directory->d_name, strlen(directory->d_name) + 1);

        if(opendir(filePath)!=NULL){
          counter += file_count(filePath);
          continue;
        }

        counter++;
    }

    closedir(dirptr);
    return counter;

}



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
  if(argv[1][1] == 'd')
    strcpy(dirName, argv[2]);
  if(argv[3][1] == 'p')
    client_port = atoi(argv[4]);
  if(argv[5][1] == 'w')
    workerThreads = atoi(argv[6]);
  if(argv[7][1] == 'b')
    POOL_SIZE = atoi(argv[8]);
  if(argv[9][1] == 's' && argv[9][2] == 'p')
    PORT = atoi(argv[10]);
  if(argv[11][1] == 's' && argv[11][2] == 'i' && argv[11][3] == 'p')
    strcpy(SERVERHOST, argv[12]);

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

  /* Send data to the server. */
  char msg[20];
  strcpy(msg, "LOG_ON ");
  write_to_server (sock,msg);
  strcpy(msg, "GET_CLIENTS ");
  write_to_server (sock,msg);
  
  initialize(&pool);
  pthread_mutex_init(&mtx, 0);
  pthread_cond_init(&cond_nonempty, 0);
  pthread_cond_init(&cond_nonfull, 0);
  
  read_from_server(sock, myIP);
  fill_pool();
  pthread_t cons, prod;

  
  for(int i = 0 ; i < workerThreads; i++)
    pthread_create(&threads[i], NULL, worker_Thread, NULL);
  while(1)
  {
    read_from_server(sock, myIP);
  }



  pthread_cond_destroy(&cond_nonempty);
  pthread_cond_destroy(&cond_nonfull);
  pthread_mutex_destroy(&mtx);
  close (sock);
  
  // sock = make_socket (client_port);
  // if (listen (sock, 1) < 0)
  // {
  //   perror ("listen");
  //   exit (EXIT_FAILURE);
  // }

  // /* Initialize the set of active sockets. */
  // FD_ZERO (&active_fd_set);
  // FD_SET (sock, &active_fd_set);

  // while (1)
  // {
  //   /* Block until input arrives on one or more active sockets. */
  //   read_fd_set = active_fd_set;
  //   if (select (FD_SETSIZE, &read_fd_set, NULL, NULL, NULL) < 0)
  //   {
  //   perror ("select");
  //   exit (EXIT_FAILURE);
  //   }

  //   /* Service all the sockets with input pending. */
  //   for (int i = 0; i < FD_SETSIZE; ++i)
  //   {
  //       if (FD_ISSET (i, &read_fd_set))
  //       {
  //           if (i == sock)
  //           {
  //               /* Connection request on original socket. */
  //               int new;
  //               size = sizeof (servername);
  //               new = accept (sock,
  //                           (struct sockaddr *) &servername,
  //                           (socklen_t *)&size);
  //               if (new < 0)
  //               {pthread_cond_signal(&cond_nonempty);
  //                pthread_cond_signal(&cond_nonempty);
  //                   exit (EXIT_FAILURE);
  //               }
  //               fprintf (stderr,
  //                       "Server: connect from host %s, port %hd.\n",
  //                       inet_ntoa (servername.sin_addr),
  //                       ntohs (servername.sin_port));
  //               FD_SET (new, &active_fd_set);
  //           }
  //           else
  //           {
  //               /* Data arriving on an already-connected socket. */
  //               if ( read_from_server (i) < 0)
  //               {
  //                   close (i);
  //                   FD_CLR (i, &active_fd_set);
  //               }

  //           }
  //       }
  //   }
  // }

  exit (EXIT_SUCCESS);
}


