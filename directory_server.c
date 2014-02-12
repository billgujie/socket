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
#define PHASE1 21374
#define PHASE2 31374
#define HOSTNAME "nunki.usc.edu"
#define ERROR -1
#define MAXSERVER 3
#define MAXCLIENT 2
#define BUFSIZE 1024
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
	if (strncmp(subbuf, "Client1", 7) == 0) {
		return 1;
	}
	if (strncmp(subbuf, "Client2", 7) == 0) {
		return 2;
	}
	printf("Can not find useful info from received message.\n");
	exit(-1);
}
/*
 * take topology and resource information and requested doc# as input
 * calculate the shortest path on available FileServers
 * Returns target File_Server port#
 */
int routing(int route[][4], int reginfo[][3], int doc) {
	int server;
	int best = 99999999;//set current lowest cost to be huge
	int target=0;
	for (server = 1; server < 4; server++) {
		if ((reginfo[server][doc] == 1) && (route[doc][server] < best)) {
			best = route[doc][server];
			target=server;
		}
	}
	//printf("For doc%d, go to File Server#%d port:%d.\n",doc,target,reginfo[target][0]);
	return target;
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
 * Function to read topology.txt and store them as a 2D array
 */
void get_topology(int route[][4]) {
	FILE * pFile;
	char buf[BUFSIZE];
	char *pch;
	int server;
	pFile = fopen("topology.txt", "r");//read mode only
	if (!pFile) {
		perror("fopen topology.txt failed");
		exit(errno);
	}
	int col, row = 1;
	while (fgets(buf, BUFSIZE, pFile) != NULL) {
		pch = strtok(buf, " ");
		col = 1;
		while (pch != NULL) {
			route[row][col++] = atoi(pch);
			pch = strtok(NULL, " \r");
		}
		row++;
	}
	fclose(pFile);
}

/*
 * Function to read resource.txt and store information into 2D array
 */
void get_resource(int reginfo[][3]) {
	FILE * pFile;
	char buf[BUFSIZE];
	char *pch;
	int server;
	pFile = fopen("resource.txt", "r");//read mode only
	if (!pFile) {
		perror("fopen resource.txt failed");
		exit(errno);
	}
	while (fgets(buf, BUFSIZE, pFile) != NULL) {// read line by line

		//get File_Server name
		pch = strtok(buf, " ");
		if (strcmp(pch, "File_Server1") == 0) {
			server = 1;
		} else if (strcmp(pch, "File_Server2") == 0) {
			server = 2;
		} else if (strcmp(pch, "File_Server3") == 0) {
			server = 3;
		} else {
			printf("Name match Failed\n");
			exit(-1);
		}
		//jump over the number of resources
		pch = strtok(NULL, " \r");
		pch = strtok(NULL, " \r");
		while (pch != NULL) {
			//printf("%s", pch);
			if (strcmp(pch, "doc1") == 0) {
				reginfo[server][1] = 1;
			}
			if (strcmp(pch, "doc2") == 0) {
				reginfo[server][2] = 1;
			}
			pch = strtok(NULL, " \r");
		}

	}
	fclose(pFile);
}
/*
 * Directory Server Phase1
 * setup UDP socket and port
 * reading incoming message from File_Servers
 * and store information into directory.txt file
 * Also perform necessary error checking and ACK to File Servers to notify the recipient of message
 */
int phase1(int reginfo[][3]) {
	int directory_sock1;
	struct sockaddr_in addr1, remote;
	struct hostent *directoryent;
	char str[INET_ADDRSTRLEN];
	char message[BUFSIZE] = "ACK";
	int recvlen; //received message length
	char buf[BUFSIZE];
	char cp[BUFSIZE];
	int ID; // file_server #ID
	int addrlen = sizeof(remote);
	memset(&addr1, 0, sizeof(addr1));
	addr1.sin_family = AF_INET; // set to AF_INET to force IPv4
	addr1.sin_port = htons(PHASE1); // staticlly assigned port number for phase 1

	// get the host info
	if ((directoryent = gethostbyname(HOSTNAME)) == NULL) {
		herror("gethostbyname returned NULL");
		exit(ERROR);
	}

	memcpy((void *) &addr1.sin_addr, directoryent->h_addr_list[0],
			directoryent->h_length);

	inet_ntop(AF_INET, &(addr1.sin_addr), str, INET_ADDRSTRLEN);

	printf(
			"Phase 1: The Directory Server has UDP port number %d and IP address %s.\n",
			PHASE1, str);

	//create UDP socket
	if ((directory_sock1 = socket(AF_INET, SOCK_DGRAM, 0)) == ERROR) {
		perror("socket creation failed");
		exit(errno);
	}

	//bind socket
	if (bind(directory_sock1, (struct sockaddr*) &addr1, sizeof(addr1))
			== ERROR) {
		perror("bind failed");
		exit(errno);
	}

	FILE * pFile;
	pFile = fopen("directory.txt", "w+");
	if (!pFile) {
		perror("fopen failed");
		exit(errno);
	}
	//printf("receiving..\n");
	int count;
	int port;
	for (count = 0; count < MAXSERVER;) {
		//waiting on port #21374
		recvlen = recvfrom(directory_sock1, buf, BUFSIZE, 0,
				(struct sockaddr *) &remote, &addrlen);
		if (recvlen > 0) {
			buf[recvlen] = 0;
			//printf("received message: \"%s\" (%d bytes)\n", buf, recvlen);
			strcpy(cp, buf);
			ID = identify(cp);
			printf(
					"Phase 1: The Directory Server has received request from File Server %d.\n",
					ID);

			strcat(buf, "\n");//adding the message with newline

			if (fputs(buf, pFile) == -1) {
				printf("fputs failed");
				exit(errno);
			}
			// send back ACK to file servers so they can end their phase1
			sendto(directory_sock1, message, strlen(message) + 1, 0,
					(struct sockaddr *) &remote, sizeof(remote));

			port = get_port(buf);
			reginfo[ID][0] = port;
			count++;
		}
	}
	close(directory_sock1);
	fclose(pFile);
	printf("Phase 1: The directory.txt file has been created.\n");
	return 0;
}

/*
 * Directory Server Phase2
 * Setup UDP socket and port
 * receive request from clients and send back calculated information
 */
int phase2(int reginfo[][3], int route[][4]) {
	int directory_sock1;
	struct sockaddr_in addr1, remote;
	struct hostent *directoryent;
	char str[INET_ADDRSTRLEN];
	char message[BUFSIZE];
	int recvlen; //received message length
	char buf[BUFSIZE];
	char cp[BUFSIZE];
	int ID; // file_server #ID
	int addrlen = sizeof(remote);
	memset(&addr1, 0, sizeof(addr1));
	addr1.sin_family = AF_INET; // set to AF_INET to force IPv4
	addr1.sin_port = htons(PHASE2); // staticlly assigned port number for phase 1

	// get the host info
	if ((directoryent = gethostbyname(HOSTNAME)) == NULL) {
		herror("gethostbyname returned NULL");
		exit(ERROR);
	}

	memcpy((void *) &addr1.sin_addr, directoryent->h_addr_list[0],
			directoryent->h_length);

	inet_ntop(AF_INET, &(addr1.sin_addr), str, INET_ADDRSTRLEN);

	printf(
			"Phase 2: The Directory Server has UDP port number %d and IP address %s.\n",
			PHASE2, str);

	//create UDP socket
	if ((directory_sock1 = socket(AF_INET, SOCK_DGRAM, 0)) == ERROR) {
		perror("socket creation failed");
		exit(errno);
	}

	//bind socket
	if (bind(directory_sock1, (struct sockaddr*) &addr1, sizeof(addr1))
			== ERROR) {
		perror("bind failed");
		exit(errno);
	}

	int count;
	int port;
	int target_server;
	for (count = 0; count < MAXCLIENT;) {
		//waiting on port #31374
		recvlen = recvfrom(directory_sock1, buf, BUFSIZE, 0,
				(struct sockaddr *) &remote, &addrlen);
		if (recvlen > 0) {
			buf[recvlen] = 0;
			//printf("received message: \"%s\" (%d bytes)\n", buf, recvlen);
			strcpy(cp, buf);
			ID = identify(cp);
			//calculate destination based on routing information
			target_server=routing(route, reginfo, ID);
			if (target_server==0){
				printf("ERROR: Requested doc%d does not exist on any of the registered servers.\n",ID);
				count++;
				continue;
			}
			printf(
					"Phase 2: The Directory Server has received request from Client %d.\n",
					ID);
			// construct the message and send back to Clients
			sprintf (message, "File_Server%d %d", target_server, reginfo[target_server][0]);
			sendto(directory_sock1, message, strlen(message) + 1, 0,
					(struct sockaddr *) &remote, sizeof(remote));
			printf("Phase 2: File server details has been sent to Client %d.\n", ID);
			count++;
		}

	}
	close(directory_sock1);
	return 0;
}

int main(void) {
	/*
	 * reginfo structure:
	 * [  empty ][  empty  ][  empty  ]
	 * [FS#1 TCP][FS#1 doc1][FS#1 doc2]
	 * [FS#2 TCP][FS#2 doc1][FS#2 doc2]
	 * [FS#3 TCP][FS#3 doc1][FS#3 doc2]
	 *
	 * route structure:
	 *          [     empty     ][     empty     ][     empty     ]
	 * [ empty ][Client1 to FS#1][Client1 to FS#2][Client1 to FS#3]
	 * [ empty ][Client2 to FS#1][Client2 to FS#2][Client2 to FS#3]
	 *
	 */
	int reginfo[4][3];
	int route[3][4];
	memset(reginfo, 0, sizeof(reginfo));

	if (phase1(reginfo) == 0) {
		printf("Phase 1: End of Phase 1 for the Directory Server.\n");
	}

	get_resource(reginfo);
	get_topology(route);

	if (phase2(reginfo, route) == 0){
		printf("Phase 2: End of Phase 2 for the Directory Server.\n");
	}

	exit(0);

}

