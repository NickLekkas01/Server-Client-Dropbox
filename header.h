typedef struct client_data
{
    struct sockaddr_in *data;
    struct client_data *next;
    int filedesc;
}Client_data;

uint32_t find_IP(Client_data *st, int filedesc);
uint32_t find_port(Client_data *st, int filedesc);
void Insert_Node(Client_data **st, in_addr_t IP, in_port_t port, int filedesc);
int Node_Exists(Client_data *st, in_addr_t IP, in_port_t port, int filedesc);
void Print_List(Client_data *st);
int Count_List(Client_data *st);
int Delete_Node(Client_data **st, uint32_t IP, uint32_t port);