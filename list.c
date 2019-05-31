#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "header.h"

uint32_t find_IP(Client_data *st, int filedesc)
{
    Client_data *temp = st;
    while(temp != NULL)
    {
        if(temp->filedesc == filedesc) 
            return temp->data->sin_addr.s_addr;
        temp = temp->next;
    }
    return 0;
}

uint32_t find_port(Client_data *st, int filedesc)
{
    Client_data *temp = st;
    while(temp != NULL)
    {
        if(temp->filedesc == filedesc) 
            return temp->data->sin_port;
        temp = temp->next;
    }
    return 0;
}

int Delete_Node(Client_data **st, uint32_t IP, uint32_t port)
{

    Client_data *temp = *st;
    Client_data *temp2;
    if((*st)->data->sin_addr.s_addr == IP && (*st)->data->sin_port == port)
    {
        temp = *st;
        *st = temp->next;
        free(temp);
        return 1;
    }
    else
    {
        while(temp->next != NULL)
        {
            if(temp->next->data->sin_addr.s_addr == IP && temp->next->data->sin_port == port)
            {
                temp2 = temp->next;
                temp->next = temp2->next;
                free(temp2);
                return 1;
            }
            temp = temp->next;
        }    
    }
    return 0;
}

void Insert_Node(Client_data **st, in_addr_t IP, in_port_t port, int filedesc)
{
    Client_data *new = malloc(sizeof(Client_data));
    new->data = malloc(sizeof(struct sockaddr_in));
    new->data->sin_family = AF_INET;
    new->data->sin_addr.s_addr = IP;
    new->data->sin_port = port;
    new->filedesc = filedesc;
    new->next = *st;
    *st = new;
}

int Node_Exists(Client_data *st, in_addr_t IP, in_port_t port, int filedesc)
{
    Client_data *temp = st;
    while(temp != NULL)
    {
        if(temp->data->sin_port == port && temp->data->sin_addr.s_addr == IP && temp->filedesc == filedesc)
            return 1;
        temp = temp->next;
    }
    return 0;
}

int Count_List(Client_data *st)
{
    Client_data *temp = st;
    int count = 0;
    while(temp != NULL)
    {
        count++;
        temp = temp->next;
    }
    return count;
}

void Print_List(Client_data *st)
{
    Client_data *temp = st;
    char IP[50];
    printf("{\n");
    while(temp != NULL)
    {
        printf("Address:%s  -  Port:%u \n", inet_ntoa(temp->data->sin_addr), temp->data->sin_port);
        temp = temp->next;
    }
    printf("}\n");
}
