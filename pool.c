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

void initialize(pool_t * pool) 
{
  pool->data = malloc(POOL_SIZE * sizeof(Pool_data));
  pool->start = 0;
  pool->end = -1;
  pool->count = 0;
}

void place(pool_t * pool, uint32_t IP, uint32_t port, time_t version, char *path) 
{
  pthread_mutex_lock(&pool_mtx);
  while (pool->count >= POOL_SIZE) 
  {
    //printf(">> Found Buffer Full \n");
    pthread_cond_wait(&cond_nonfull, &pool_mtx);
  }
  pool->end = (pool->end + 1) % POOL_SIZE;
  pool->data[pool->end].IP = IP;
  pool->data[pool->end].port = port;
  pool->data[pool->end].version =  version;
  strcpy(pool->data[pool->end].path, path);
  pool->count++;
  pthread_mutex_unlock(&pool_mtx);
}

Pool_data *obtain(pool_t * pool) 
{
  Pool_data *data;
  data = malloc(sizeof(Pool_data));
  pthread_mutex_lock(&pool_mtx);
  while (pool->count <= 0) 
  {
    //printf(">> Found Buffer Empty \n");
    pthread_cond_wait(&cond_nonempty, &pool_mtx);
  }
  data->IP = pool->data[pool->start].IP;
  data->port = pool->data[pool->start].port;
  strcpy(data->path, pool->data[pool->start].path);
  data->version = pool->data[pool->start].version;
  pool->start = (pool->start + 1) % POOL_SIZE;
  pool->count--;
  pthread_mutex_unlock(&pool_mtx);
  return data;
}

void fill_pool(Client_data *start_client_list) 
{
  Client_data *temp = start_client_list;
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