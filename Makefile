all: dropbox_server.o dropbox_client.o list.o pool.o
	gcc dropbox_server.o list.o -o server
	gcc dropbox_client.o pool.o list.c -o client -lpthread

objects: list.c dropbox_server.c dropbox_client.c pool.c
	gcc list.c -c
	gcc dropbox_server.c -c
	gcc dropbox_client.c -c
	gcc pool.c -c

clean: server client pool.o dropbox_server.o dropbox_client.o
	rm server client dropbox_server.o dropbox_client.o pool.o list.o
