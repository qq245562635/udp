#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#include "udp.h"

struct sockaddr_in serverAddr;

int sock;
pthread_t recvThread;
void *status;

void signal_handler(int sig) {
	printf("signal_handler...\n");
	shutdown(sock, SHUT_RDWR);
	close(sock);
	pthread_join(recvThread, &status);
	exit(0);
}

void *recv_handler(void *arg) {
	char buff[512];
	socklen_t len = 0;
	int n;

	printf("recv_handler...\n");
	while(1) {
		n = recvfrom(sock, buff, 511, 0, (struct sockaddr*) &serverAddr, &len);
		if(n>0) {
			buff[n] = 0;
			printf("recvfrom(%3d): %s", n, buff);
		} else {
			perror("recvfrom");
			break;
		}
	}
}

int main(int argc, char **argv) {
	char buff[512];
	char *ip = "127.0.0.1";
	socklen_t len = sizeof(struct sockaddr_in);
	
	if(argc == 2) {
		ip = argv[1];
	}

	if((sock=socket(AF_INET, SOCK_DGRAM, 0)) <0) {  
		perror("socket");
		return 1;
	}
	
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(3702);
	serverAddr.sin_addr.s_addr = inet_addr(ip);
	if(serverAddr.sin_addr.s_addr == INADDR_NONE) {
		perror("Incorrect ip serverAddress!");
		close(sock);
		return 1;
	}

	signal(SIGINT, signal_handler);

	printf("This is a UDP client\n");
	
	pthread_create(&recvThread, NULL, recv_handler, NULL);
	for(;;) {
		// 接受用户输入
		bzero(buff, 512);
		
		//从键盘输入内容
		if(fgets(buff, 511, stdin) != buff) {
			printf("\nEOF\n");
			break;
		}
		
		if(sendto(sock, buff, strlen(buff), 0, (struct sockaddr *) &serverAddr, len) < 0) {
			perror("sendto");
			break;
		}
	}
	
	printf("close...\n");
	shutdown(sock, SHUT_RDWR);
	close(sock);
	
	pthread_join(recvThread, &status);

	return 0;
}

