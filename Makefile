CC=gcc
CFLAGS = -g 
# uncomment this for SunOS
 LIBS = -lsocket -lnsl -lresolv

all: file_server1 file_server2 file_server3 client1 client2 directory_server

file_server1: file_server1.o 
	$(CC) -o file_server1 file_server1.o $(LIBS)

file_server2: file_server2.o 
	$(CC) -o file_server2 file_server2.o $(LIBS)
	
file_server3: file_server3.o 
	$(CC) -o file_server3 file_server3.o $(LIBS)	

client1: client1.o
	$(CC) -o client1 client1.o $(LIBS)
	
client2: client2.o
	$(CC) -o client2 client2.o $(LIBS)

directory_server: directory_server.o 
	$(CC) -o directory_server directory_server.o $(LIBS)

file_server1.o: file_server1.c

file_server2.o: file_server2.c

file_server3.o: file_server3.c

client1.o: client1.c

client2.o: client2.c

directory_server.o: directory_server.c

clean:
	rm -f directory_server file_server1 file_server2 file_server3 directory_server.o file_server1.o file_server2.o file_server3.o client1 client1.o client2 client2.o
