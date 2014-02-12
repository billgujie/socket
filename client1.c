#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#define PHASE2 32374
#define HOSTNAME "nunki.usc.edu"
#define ERROR -1
#define BUFSIZE 1024
#define DIRSERV 31374
/*
 * function to grab the first part of incoming message and determine their origin
 * return the identity as a number
 */
int identify(char buf[BUFSIZE]) {
	char *subbuf;
	subbuf = strtok(buf, " ");
	if (strncmp(subbuf, "File_Server1", 12) == 0) {
		return 1;
	}
	if (strncmp(subbuf, "File_Server2", 12) == 0) {
		return 2;
	}
	if (strncmp(subbuf, "File_Server3", 12) == 0) {
		return 3;
	}
	printf("Can not find useful info from received message.\n");
	exit(-1);
}
/*
 * read incoming message from File_servers
 * return their TCP port stated in message
 */
int get_port(char buf[BUFSIZE]) {
	int port;
	char *subbuf;
	subbuf = strtok(buf, " ");
	subbuf = strtok(NULL, " ");
	port = atoi(subbuf);
	return port;
}
/*
 * Client Phase2
 * Setup UDP socket/port and send file request to Directory_server
 * And save the received remote Server and its port into Array
 */
int phase2(int remote_info[2]) {
	int udp_sock;
	struct sockaddr_in addr1, remote;
	struct hostent *selfent;
	char readableIP[INET_ADDRSTRLEN];
	char *message = "Client1 doc1";
	int recvlen; //received message length
	char buf[BUFSIZE];
	char cp[BUFSIZE];
	int addrlen = sizeof(remote);

	memset(&addr1, 0, sizeof(addr1));
	addr1.sin_family = AF_INET; // set to AF_INET to force IPv4
	addr1.sin_port = htons(PHASE2); // staticlly assigned port number for phase 2

	memset(&remote, 0, sizeof(remote));
	remote.sin_family = AF_INET; // set to AF_INET to force IPv4
	remote.sin_port = htons(DIRSERV); // staticlly assigned port number for phase 1

	// get the host info
	if ((selfent = gethostbyname(HOSTNAME)) == NULL) {
		herror("gethostbyname returned NULL");
		exit(ERROR);
	}

	memcpy((void *) &addr1.sin_addr, selfent->h_addr_list[0], selfent->h_length);
	memcpy((void *) &remote.sin_addr, selfent->h_addr_list[0],
			selfent->h_length);
	inet_ntop(AF_INET, &(addr1.sin_addr), readableIP, INET_ADDRSTRLEN);

	//create UDP socket
	if ((udp_sock = socket(AF_INET, SOCK_DGRAM, 0)) == ERROR) {
		perror("socket creation failed");
		exit(errno);
	}

	//bind socket
	if (bind(udp_sock, (struct sockaddr*) &addr1, sizeof(addr1)) == ERROR) {
		perror("bind failed");
		exit(errno);
	}
	printf("Phase 2: Client 1 has UDP port number %d and IP address %s.\n",
			PHASE2, readableIP);

	//send message
	if (sendto(udp_sock, message, strlen(message) + 1, 0,
			(struct sockaddr *) &remote, sizeof(remote)) <= 0) {
		perror("sendto failed");
		exit(errno);
	}
	printf(
			"Phase 2: The File request from Client 1 has been sent to the Directory Server.\n");

	//loop, receiving data
	int file_server;
	int server_tcp;
	for (;;) {
		//waiting on port #32374
		recvlen = recvfrom(udp_sock, buf, BUFSIZE, 0,
				(struct sockaddr *) &remote, &addrlen);
		if (recvlen > 0) {
			buf[recvlen] = 0;
			strcpy(cp, buf);
			//Identify the target file server and its TCP port#
			file_server = identify(cp);
			server_tcp = get_port(buf);
			printf(
					"Phase 2: The File requested by Client 1 is present in File Server %d and the File Server's TCP port number is %d.\n",
					file_server, server_tcp);
			close(udp_sock);
			printf("Phase 2: End of Phase 2 for Client 1.\n");
			remote_info[0] = file_server;
			remote_info[1] = server_tcp;
			return 0;
		}
	}
	exit(-1);
}
/*
 * Client Phase3
 * Setup dynamic TCP socket/port and send request to Target FileServer
 * Upon receiving the doc string close the Client
 */
int phase3(int remote_info[2]) {
	int tcp_sock;
	struct sockaddr_in self_addr, remote, temp;
	struct hostent *selfent;
	char readableIP[INET_ADDRSTRLEN];
	int recvlen; //received message length
	char buf[BUFSIZE];
	char *message="Client1 doc1";
	int addrlen = sizeof(remote);
	int getsock_len;

	memset(&self_addr, 0, sizeof(self_addr));
	self_addr.sin_family = AF_INET; // set to AF_INET to force IPv4

	memset(&remote, 0, sizeof(remote));
	remote.sin_family = AF_INET; // set to AF_INET to force IPv4
	remote.sin_port = htons(remote_info[1]); // TCP port number in File Server

	// get the host info
	if ((selfent = gethostbyname(HOSTNAME)) == NULL) {
		herror("gethostbyname returned NULL");
		exit(ERROR);
	}

	memcpy((void *) &self_addr.sin_addr, selfent->h_addr_list[0],
			selfent->h_length);
	memcpy((void *) &remote.sin_addr, selfent->h_addr_list[0],
			selfent->h_length);
	inet_ntop(AF_INET, &(self_addr.sin_addr), readableIP, INET_ADDRSTRLEN);

	//create client socket
	if ((tcp_sock = socket(AF_INET, SOCK_STREAM, 0)) == ERROR) {
		perror("TCP socket Failed");
		exit(errno);
	}

	//bind socket may not necessary
	if (bind(tcp_sock, (struct sockaddr *) &self_addr, sizeof(self_addr))
			== ERROR) {
		perror("bind failed");
		exit(errno);
	}
	// get locally bound TCP socket
	if (getsockname(tcp_sock, (struct sockaddr *) &self_addr, &getsock_len)
			== ERROR) {
		perror("get local sock failed");
		exit(errno);
	}

	//connect to server
	if ((connect(tcp_sock, (struct sockaddr *) &remote, addrlen)) == ERROR) {
		close(tcp_sock);
		perror("Connect Failed");
		exit(errno);
	}

	printf(
			"Phase 3: Client 1 has dynamic TCP port number %d and IP address %s.\n",
			self_addr.sin_port, readableIP);
	//strcpy(message, "doc1 ");
	send(tcp_sock, message, strlen(message)+1, 0);
	printf(
			"Phase 3: The File request from Client 1 has been sent to the File Server %d.\n",
			remote_info[0]);
	for (;;) {
		recvlen = recv(tcp_sock, buf, BUFSIZE, 0);
		if (recvlen > 0) {
			buf[recvlen] = 0;
			//printf("%s\n", buf);
			if (strncmp(buf, "doc1", 4) == 0) {
				//shutdown(tcp_sock, 2);
				close(tcp_sock);
				printf(
						"Phase 3: Client 1 received doc1 from File Server %d.\n",
						remote_info[0]);
				printf("Phase 3: End of Phase 3 for Client 1.\n");
				return 0;
			}
		}
	}

	return 0;
}

int main(void) {
	int remote_info[2];

	phase2(remote_info);
	phase3(remote_info);

	exit(0);

}
