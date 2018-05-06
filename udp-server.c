#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "udp.h"

int sock;

void signal_handler(int sig) {
	printf("signal_handler...\n");
	close(sock);
	exit(0);
}

int main(int argc, char *argv[]) {
	struct sockaddr_in bindAddr;
	struct sockaddr_in clientAddr;
	int n;
	socklen_t len = sizeof(struct sockaddr_in);
	char buff[512];

	bindAddr.sin_family = AF_INET;
	bindAddr.sin_port = htons(3702);
	bindAddr.sin_addr.s_addr = htonl(INADDR_ANY);

	if((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("socket");
		return 1;
	}
	if(bind(sock, (struct sockaddr*) &bindAddr, len) < 0) {
		perror("bind");
		return 1;
	}
	
	signal(SIGINT, signal_handler);

	printf("Welcome! This is a UDP server, I can only received message from client and reply with same message\n");

	for(;;) {
		bzero(buff, 512);
		n = recvfrom(sock, buff, 511, 0, (struct sockaddr*) &clientAddr, &len);
		if(n>0) {
			buff[n] = 0;
			printf("%s %u recvfrom(%3d): %s", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port), n, buff);
			n = sendto(sock, buff, n, 0, (struct sockaddr*) &clientAddr, len);
			if(n <= 0) {
				perror("sendto");
				break;
			}
		} else {
			perror("recv");
			break;
		}
	}

	printf("close...\n");
	close(sock);

	return 0;
}

