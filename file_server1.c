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
#define SERVER 21374
#define PHASE1 22374
#define PHASE3 41374
#define HOSTNAME "nunki.usc.edu"
#define ERROR -1
#define BUFSIZE 1024
/*
 * function to grab the first part of incoming message and determine their origin
 * return the identity as a number
 */
int identify(char buf[BUFSIZE]) {
	if (strncmp(buf, "Client1", 7) == 0) {
		return 1;
	}
	if (strncmp(buf, "Client2", 7) == 0) {
		return 2;
	}
	printf("Can not find useful info from received message.\n");
	exit(-1);
}
/*
 * FileServer Phase1
 * Setup UDP socket/port and send information to Directory Server
 * And end Phase1 if received ACK from directory server
 */
int phase1(void) {
	int udp_sock;
	struct sockaddr_in self, remote;
	struct hostent *selfent;
	char str[INET_ADDRSTRLEN];
	char *message = "File_Server1 41374";
	char *hostname = "nunki.usc.edu";
	int recvlen; //received message length
	char buf[BUFSIZE];
	int addrlen = sizeof(remote);
	memset(&self, 0, sizeof(self));
	self.sin_family = AF_INET; // set to AF_INET to force IPv4
	self.sin_port = htons(PHASE1); // staticlly assigned port number for phase 1

	memset(&remote, 0, sizeof(remote));
	remote.sin_family = AF_INET; // set to AF_INET to force IPv4
	remote.sin_port = htons(SERVER); // staticlly assigned port number for phase 1

	// get the host info
	if ((selfent = gethostbyname(HOSTNAME)) == NULL) {
		herror("gethostbyname returned NULL");
		exit(ERROR);
	}

	memcpy((void *) &self.sin_addr, selfent->h_addr_list[0], selfent->h_length);
	memcpy((void *) &remote.sin_addr, selfent->h_addr_list[0],
			selfent->h_length);

	inet_ntop(AF_INET, &(self.sin_addr), str, INET_ADDRSTRLEN);

	//create UDP socket
	if ((udp_sock = socket(AF_INET, SOCK_DGRAM, 0)) == ERROR) {
		perror("socket creation failed");
		exit(errno);
	}

	//bind socket
	if (bind(udp_sock, (struct sockaddr*) &self, sizeof(self)) == ERROR) {
		perror("bind failed");
		exit(errno);
	}
	printf(
			"Phase 1: File Server 1 has UDP port number %d and IP address %s.\n",
			PHASE1, str);

	//send message
	if (sendto(udp_sock, message, strlen(message) + 1, 0,
			(struct sockaddr *) &remote, sizeof(remote)) == ERROR) {
		perror("sendto failed");
		exit(errno);
	}
	printf(
			"Phase 1: The Registration request from File Server 1 has been sent to the Directory Server.\n");

	//loop, receiving data
	for (;;) {
		//waiting on port #22374
		recvlen = recvfrom(udp_sock, buf, BUFSIZE, 0,
				(struct sockaddr *) &remote, &addrlen);

		if (recvlen > 0) {
			buf[recvlen] = 0;
			//printf("received message: %s.\n", buf);
			//If the message back from directory is ACK then ends Phase1
			if (strncmp(buf, "ACK", 3) == 0) {
				close(udp_sock);
				return 0;
			}
		}
	}
}
/*
 * FileServer Phase3
 * Setup TCP socket/port and listen any incoming request from Client
 * And send back the doc requested from Client
 * User must end this by Ctrl+C because as required by project FileServer need keep listening
 */
int phase3(void) {
	int tcp_sock, incoming;
	struct sockaddr_in self_addr, remote;
	struct sockaddr_storage their_addr;
	struct hostent *selfent;
	char readableIP[INET_ADDRSTRLEN];
	int recvlen; //received message length
	char buf[BUFSIZE];
	char message[BUFSIZE];
	int addrlen = sizeof(remote);
	int getsock_len;
	int client;
	socklen_t sin_size;

	memset(&self_addr, 0, sizeof(self_addr));
	self_addr.sin_family = AF_INET; // set to AF_INET to force IPv4
	self_addr.sin_port = htons(PHASE3);

	//memset(&remote, 0, sizeof(remote));
	//remote.sin_family = AF_INET; // set to AF_INET to force IPv4
	//remote.sin_port = htons(remote_info[1]); // TCP port number in File Server

	// get the host info
	if ((selfent = gethostbyname(HOSTNAME)) == NULL) {
		herror("gethostbyname returned NULL");
		exit(ERROR);
	}

	memcpy((void *) &self_addr.sin_addr, selfent->h_addr_list[0],
			selfent->h_length);
	//memcpy((void *) &remote.sin_addr, selfent->h_addr_list[0],
	//		selfent->h_length);
	inet_ntop(AF_INET, &(self_addr.sin_addr), readableIP, INET_ADDRSTRLEN);

	//create client socket
	if ((tcp_sock = socket(AF_INET, SOCK_STREAM, 0)) == ERROR) {
		perror("TCP socket Failed");
		exit(errno);
	}

	//bind socket
	if (bind(tcp_sock, (struct sockaddr *) &self_addr, sizeof(self_addr))
			== ERROR) {
		perror("bind failed");
		exit(errno);
	}

	//start listen with queue capacity of 5
	if (listen(tcp_sock, 5) == ERROR) {
		perror("listen failed");
		exit(errno);
	}

	printf("Phase 3: File Server 1 has TCP port %d and IP address %s.\n",
			PHASE3, readableIP);

	//keep listening
	while (1) {
		sin_size = sizeof(their_addr);
		incoming = accept(tcp_sock, (struct sockaddr *) &their_addr, &sin_size);
		if (incoming == ERROR) {
			perror("accept failed");
			exit(errno);
		}

		recvlen = recv(incoming, buf, BUFSIZE, 0);
		if (recvlen > 0) {
			buf[recvlen] = 0;
			//identify the requesting Client number
			client = identify(buf);
			printf(
					"Phase 3: File Server 1 received the request from the Client %d for the file doc%d.\n",
					client, client);

			//construct the doc string and send back to Client
			sprintf(message, "doc%d", client);
			send(incoming, message, strlen(message) + 1, 0);
			printf("Phase 3: File Server 1 has sent doc%d to Client %d.\n",
					client, client);
			//close(incoming);
		}
	}

	return 0;
}

int main(void) {
	if (phase1() == 0) {
		printf("Phase 1: End of Phase 1 for File Server 1.\n");
	}

	phase3();

	exit(0);
}
