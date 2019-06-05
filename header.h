typedef struct client_data
{
    struct sockaddr_in *data;
    struct client_data *next;
    int filedesc;
}Client_data;

uint32_t myIP;
uint32_t client_port;
int server_sock;
int listeningSock;

int POOL_SIZE;
int PORT;
char dirName[40];


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

pthread_mutex_t list_mtx;
pthread_mutex_t pool_mtx;
pthread_cond_t cond_nonempty;
pthread_cond_t cond_nonfull;
pool_t pool;

void initialize(pool_t * pool);
void place(pool_t * pool, uint32_t IP, uint32_t port, time_t version, char *path) ;
Pool_data *obtain(pool_t * pool) ;
void fill_pool(Client_data *start_client_list) ;
uint32_t find_IP(Client_data *st, int filedesc);
uint32_t find_port(Client_data *st, int filedesc);
void Insert_Node(Client_data **st, in_addr_t IP, in_port_t port, int filedesc);
int Node_Exists(Client_data *st, in_addr_t IP, in_port_t port, int filedesc);
void Print_List(Client_data *st);
int Count_List(Client_data *st);
int Delete_Node(Client_data **st, uint32_t IP, uint32_t port);