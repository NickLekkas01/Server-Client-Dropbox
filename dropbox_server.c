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


#define PORT    5555
#define MAXMSG  512


Client_data *start = NULL;


void init_sockaddr_by_IP (struct sockaddr_in *name,
               struct in_addr *IP,
               uint16_t port)
{
  struct hostent *hostinfo;

  name->sin_family = AF_INET;
  name->sin_port = htons (port);
  
  name->sin_addr = *(struct in_addr *)IP;
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


int send_to_clients(uint32_t IP, uint32_t port, char *msg)
{
    Client_data *temp = start;
    int nbytes;
    int sock;
    struct sockaddr_in clientname;
    while(temp != NULL)
    {
        struct in_addr a, b;
        a.s_addr = IP;
        b.s_addr = temp->data->sin_addr.s_addr;
        uint32_t temp1, temp2;
        if(temp->data->sin_addr.s_addr != IP && temp->data->sin_port != port)
        {
            while((nbytes = write (temp->filedesc, msg, strlen (msg) + 1))<=0){}
            if (nbytes < 0)
            {
                perror ("write");
                exit (EXIT_FAILURE);
            }
            temp1 = ntohl(IP);
            while((nbytes = write(temp->filedesc, &temp1, sizeof(uint32_t))<=0)){}
            if (nbytes < 0)
            {
                perror ("write");
                exit (EXIT_FAILURE);
            }
            temp2 = ntohl(port);
            while((nbytes = write(temp->filedesc, &temp2, sizeof(uint32_t))<=0)){}
            if (nbytes < 0)
            {
                perror ("write");
                exit (EXIT_FAILURE);
            }
        }
        temp = temp->next;
    }
}


int read_from_client (int filedes, struct sockaddr_in clientname, int sock)
{
    char buffer[MAXMSG];
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
    if(strcmp(buffer, "LOG_ON") == 0)
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
            /* Data read. */
            fprintf (stderr, "Server: got message: `%s'\n", buffer);
            uint32_t IP, port;
            nbytes = read (filedes, &IP, sizeof(uint32_t));
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
                IP = htonl(IP);
                struct in_addr a;
                a.s_addr = IP;        
                fprintf (stderr, "Server: got IP: `%s'\n", inet_ntoa(a));
                nbytes = read (filedes, &port, sizeof(uint32_t));
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
                    port = htonl(port);
                    fprintf (stderr, "Server: got port: `%u'\n", port);
                    if(Node_Exists(start, IP, port, filedes) == 0)
                        Insert_Node(&start, IP, port, filedes);
                
                }
            }
            Print_List(start);
            send_to_clients(IP, port, "USER_ON ");
        }
        return 0;
    }
    else if(strcmp(buffer, "GET_CLIENTS") == 0)
    {
        nbytes = write (filedes, "CLIENT_LIST ", strlen ("CLIENT_LIST ") + 1);
        if (nbytes < 0)
        {
            perror ("write");
            exit (EXIT_FAILURE);
        }
        int N = Count_List(start);
        nbytes = write (filedes, &N, sizeof(int));
        Client_data *temp = start;
        uint32_t temp1, temp2;
        for(int i = 0; i < N; i++)
        {
            temp1 = ntohl(temp->data->sin_addr.s_addr);
            temp2 = ntohl(temp->data->sin_port);
            write(filedes, &temp1, sizeof(uint32_t));
            write(filedes, &temp2, sizeof(uint32_t));

            temp = temp->next;
        }
    }
    else if(strcmp(buffer, "LOG_OFF") == 0)
    {
        uint32_t tempIP, tempport;
        tempIP = find_IP(start, filedes);
        tempport = find_port(start, filedes);
        send_to_clients(tempIP, tempport, "USER_OFF ");
        if(Delete_Node(&start, tempIP, tempport) == 0)
        {
            perror("ERROR_IP_PORT_NOT_FOUND_IN_LIST");
            exit (EXIT_FAILURE);
        }
        printf("LOG_OFF\n");
        Print_List(start);
    }
}

int main(int argc, char *argv[])
{
    int portNum;
    if (argc == 3)
    {
        portNum = atoi(argv[2]);
    }
    else
    {
        printf("Not enough arguments\n");
    }
    extern int make_socket (uint16_t port);
    int sock;
    fd_set active_fd_set, read_fd_set;
    int i;
    struct sockaddr_in clientname;
    size_t size;
    int reuse_addr=1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr));
    /* Create the socket and set it up to accept connections. */
    sock = make_socket (portNum);
    if (listen (sock, 1) < 0)
        {
        perror ("listen");
        exit (EXIT_FAILURE);
        }

    
    /* Initialize the set of active sockets. */
    FD_ZERO (&active_fd_set);
    FD_SET (sock, &active_fd_set);
    

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
        for (i = 0; i < FD_SETSIZE; ++i)
        {
            if (FD_ISSET (i, &read_fd_set))
            {
                if (i == sock)
                {
                    /* Connection request on original socket. */
                    int new;
                    size = sizeof (clientname);
                    new = accept (sock,
                                (struct sockaddr *) &clientname,
                                (socklen_t *)&size);
                    if (new < 0)
                    {
                        perror ("accept");
                        exit (EXIT_FAILURE);
                    }
                    fprintf (stderr,
                            "Server: connect from host %s, port %hd.\n",
                            inet_ntoa (clientname.sin_addr),
                            ntohs (clientname.sin_port));
                    FD_SET (new, &active_fd_set);
                }
                else
                {
                    /* Data arriving on an already-connected socket. */
                    if (read_from_client (i, clientname, sock) < 0)
                    {
                        close (i);
                        FD_CLR (i, &active_fd_set);
                    }

                    
                    
                }
            }
        }
    }
}