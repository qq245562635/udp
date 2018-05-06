#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <iostream>
#include <error.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "common.h"

using namespace std;

#define COMMANDMAXC 256
#define MAXRETRY	5
// Client登录时向服务器发送的消息

unsigned short g_nClientPort = 9896;
unsigned short g_nServerPort = SERVER_PORT;

typedef list<stUserListNode *> UserList;
UserList ClientList;

int  PrimaryUDP;
char UserName[10];
char ServerIP[20];
 
bool RecvedACK;

int mksock(int type) {
	int sock = socket(AF_INET, type, 0);
	if (sock < 0) {
		printf("create socket error");
		return -1;
	}
	return sock;
}

stUserListNode GetUser(char *username) {
	for (UserList::iterator UserIterator = ClientList.begin(); UserIterator != ClientList.end(); ++UserIterator) {
		if (strcmp(((*UserIterator)->userName), username) == 0) {
			return *(*UserIterator);
		}
	}
}

void BindSock(int sock) {
	sockaddr_in sin;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_family = AF_INET;
	sin.sin_port = htons(g_nClientPort);

	if (bind(sock, (struct sockaddr*) &sin, sizeof(sin)) < 0) {
		  return;
	}
}

unsigned int GetLocalIp() {
	struct ifreq ifr;
	strncpy(ifr.ifr_name, "eth0", IFNAMSIZ);
	ifr.ifr_name[IFNAMSIZ - 1] = 0;

	if ( 0 > ioctl( PrimaryUDP, SIOCGIFADDR, &ifr) ) {
	  perror("ioctl");
	  return -1;
	}
	return ntohl( (( sockaddr_in*)&ifr.ifr_addr )->sin_addr.s_addr) ;
}

void ConnectToServer(int sock, char *username, char *serverip) {
	sockaddr_in remote;
	remote.sin_addr.s_addr = inet_addr(serverip);
	remote.sin_family = AF_INET;
	remote.sin_port = htons(g_nServerPort);
 
	stMessage sendbuf;
	sendbuf.iMessageType = LOGIN;
	strncpy(sendbuf.message.loginmember.userName, username, 10);
	sendbuf.message.loginmember.privateIp = GetLocalIp();
	 
	in_addr tmp;
	tmp.s_addr = htonl(sendbuf.message.loginmember.privateIp);
	printf( "localIp: %s\n",inet_ntoa( tmp ) );
	 
	sendto(sock, (const char*) &sendbuf, sizeof(sendbuf), 0, (const sockaddr*) &remote, sizeof(remote));
	int usercount;
	int fromlen = sizeof(remote);
 
	for (;;) {
		fd_set readfds;
		fd_set writefds;

		FD_ZERO(&readfds);
		FD_ZERO(&writefds);

		FD_SET(sock, &readfds);
		int maxfd = sock;

		timeval to;
		to.tv_sec = 2;
		to.tv_usec = 0;

		int n = select(maxfd + 1, &readfds, &writefds, NULL, &to);
 
		if (n > 0) {
			if (FD_ISSET( sock, &readfds )) {
				int iread = recvfrom( sock, (char *) &usercount, sizeof(int), 0, (sockaddr *) &remote,(socklen_t*) &fromlen);
				if (iread <= 0) {
				  return;
				}

				break;
			}
		} else if (n < 0) {
		  return ;
		}

		sendto(sock, (const char*) &sendbuf, sizeof(sendbuf), 0, (const sockaddr*) &remote, sizeof(remote));
	}

	// 登录到服务端后，接收服务端发来的已经登录的用户的信息
	cout << "Have " << usercount << " users logined server:" << endl;
	for (int i = 0; i < usercount; i++) {
		stUserListNode *node = new stUserListNode;
		recvfrom(sock, (char*) node, sizeof(stUserListNode), 0, (sockaddr *) &remote,(socklen_t*) &fromlen);
		ClientList.push_back(node);
		cout << "Username:" << node->userName << endl;
		in_addr tmp;
		tmp.s_addr = htonl(node->ip);
		cout << "UserIP:" << inet_ntoa(tmp) << endl;
		cout << "UserPort:" << node->port << endl;
		cout << "" << endl;
	}
}

void OutputUsage() {
	cout << "You can input you command:\n"
			<< "Command Type:\"send\",\"tell\", \"exit\",\"getu\"\n"
			<< "Example: send Username Message\n"
			<< "Example: tell Username ip:port Message\n" << "         exit\n"
			<< "         getu\n" << endl;
}

/* 这是主要的函数：发送一个消息给某个用户(C)
 *流程：直接向某个用户的外网IP发送消息，如果此前没有联系过
 *	  那么此消息将无法发送，发送端等待超时。
 *	  超时后，发送端将发送一个请求信息到服务端，
 *	  要求服务端发送给客户C一个请求，请求C给本机发送打洞消息
 *	  以上流程将重复MAXRETRY次
 */
bool SendMessageTo(char *userName, char *Message) {
	char realmessage[256];
	unsigned int UserIP;
	unsigned short UserPort;
	unsigned int UserPrivateIp;
	unsigned int LocalPublicIp;
	bool FindUser = false;
	for (UserList::iterator UserIterator = ClientList.begin(); UserIterator != ClientList.end(); ++UserIterator) {
		if (strcmp(((*UserIterator)->userName), userName) == 0) { //查找要联系用户
			UserIP = (*UserIterator)->ip;
			UserPort = (*UserIterator)->port;
			UserPrivateIp = (*UserIterator)->privateIp;
			FindUser = true;
		}

		if (strcmp(((*UserIterator)->userName), UserName) == 0) { //查找要联系用户
			LocalPublicIp = (*UserIterator)->ip;
		}
	}

	if (!FindUser)
		return false;

	strcpy(realmessage, Message);

	for (int i = 0; i < MAXRETRY; i++) {
		RecvedACK = false;

		sockaddr_in remote;
		if( LocalPublicIp == UserIP ) {
			remote.sin_addr.s_addr = htonl(UserPrivateIp); //如果公网IP相同就用私网IPz直接通信
			remote.sin_port = htons(g_nClientPort);
		} else {
			remote.sin_addr.s_addr = htonl(UserIP);
			remote.sin_port = htons(UserPort);
		}
		remote.sin_family = AF_INET;

		stP2PMessage MessageHead;
		MessageHead.iMessageType = P2PMESSAGE;
		MessageHead.iStringLen = (int) strlen(realmessage) + 1;
		int isend = sendto(PrimaryUDP, (const char *) &MessageHead, sizeof(MessageHead), 0, (const sockaddr*) &remote, sizeof(remote));
		isend = sendto(PrimaryUDP, (const char *) &realmessage, MessageHead.iStringLen, 0, (const sockaddr*) &remote, sizeof(remote));

		// 等待接收线程将此标记修改
		for (int j = 0; j < 10; j++) {
			if (RecvedACK)
				return true;
			else
				usleep(300000);
		}

		// 没有接收到目标主机的回应，认为目标主机的端口映射没有
		// 打开，那么发送请求信息给服务器，要服务器告诉目标主机
		// 打开映射端口（UDP打洞）
		sockaddr_in server;
		server.sin_addr.s_addr = inet_addr(ServerIP);
		server.sin_family = AF_INET;
		server.sin_port = htons(g_nServerPort);

		stMessage transMessage;
		transMessage.iMessageType = P2PTRANS;
		strcpy(transMessage.message.translatemessage.userName, userName);
 
		sendto(PrimaryUDP, (const char*) &transMessage, sizeof(transMessage), 0, (const sockaddr*) &server, sizeof(server));
		usleep(100000);	 // 等待对方先发送信息。
	}
	return false;
}

// 解析命令，暂时只有exit和send命令
// 新增getu命令，获取当前服务器的所有用户
void ParseCommand(char * CommandLine) {
	if (strlen(CommandLine) < 4)
		return;
	char Command[10];
	strncpy(Command, CommandLine, 4);
	Command[4] = '\0';

	if (strcmp(Command, "exit") == 0) {
		stMessage sendbuf;
		sendbuf.iMessageType = LOGOUT;
		strncpy(sendbuf.message.logoutmember.userName, UserName, 10);
		sockaddr_in server;
		server.sin_addr.s_addr = inet_addr(ServerIP);
		server.sin_family = AF_INET;
		server.sin_port = htons(g_nServerPort);

		sendto(PrimaryUDP, (const char*) &sendbuf, sizeof(sendbuf), 0, (const sockaddr *) &server, sizeof(server));
		shutdown(PrimaryUDP, 2);
		close(PrimaryUDP);
		exit(0);
	} else if (strcmp(Command, "send") == 0) {
		char sendname[20];
		char message[COMMANDMAXC];
		int i;
		for (i = 5;; i++) {
			if (CommandLine[i] != ' ')
				sendname[i - 5] = CommandLine[i];
			else {
				sendname[i - 5] = '\0';
				break;
			}
		}
		strcpy(message, &(CommandLine[i + 1]));
		if (SendMessageTo(sendname, message))
			printf("Send OK!\n");
		else
			printf("Send Failure!\n");
	} else if (strcmp(Command, "getu") == 0) {
		int command = GETALLUSER;
		sockaddr_in server;
		server.sin_addr.s_addr = inet_addr(ServerIP);
		server.sin_family = AF_INET;
		server.sin_port = htons(g_nServerPort);

		sendto(PrimaryUDP, (const char*) &command, sizeof(command), 0, (const sockaddr *) &server, sizeof(server));
	}
}

// 接受消息线程
void*  RecvThreadProc( void* lpParameter) {
	sockaddr_in remote;
	int sinlen = sizeof(remote);
	stP2PMessage recvbuf;
	for (;;) {
		int iread = recvfrom(PrimaryUDP, (char *) &recvbuf, sizeof(recvbuf), 0, (sockaddr *) &remote,(socklen_t*) &sinlen);
		if (iread <= 0) {
			printf("recv error\n");
			continue;
		}
		switch (recvbuf.iMessageType) {
			case P2PMESSAGE: {
				// 接收到P2P的消息
				char *comemessage = new char[recvbuf.iStringLen];
				int iread1 = recvfrom(PrimaryUDP, comemessage, 256, 0, (sockaddr *) &remote,(socklen_t*) &sinlen);
				comemessage[iread1 - 1] = '\0';
				if (iread1 <= 0)
				  return NULL;
				else {
					printf("Recv a Message, %s:%ld -> %s\n", inet_ntoa(remote.sin_addr), htons(remote.sin_port), comemessage);
	 
					stP2PMessage sendbuf;
					sendbuf.iMessageType = P2PMESSAGEACK;
					sendto(PrimaryUDP, (const char*) &sendbuf, sizeof(sendbuf), 0, (const sockaddr*) &remote, sizeof(remote));
					printf("Send a Message Ack to %s:%ld\n", inet_ntoa(remote.sin_addr), htons(remote.sin_port));
				}

				delete[] comemessage;
				break;
			}
			case P2PSOMEONEWANTTOCALLYOU: {
				// 接收到打洞命令，向指定的IP地址打洞
				printf("Recv p2someonewanttocallyou from %s:%ld\n", inet_ntoa(remote.sin_addr), htons(remote.sin_port));

				sockaddr_in remote;
				remote.sin_addr.s_addr = htonl(recvbuf.iStringLen);
				remote.sin_family = AF_INET;
				remote.sin_port = htons(recvbuf.Port);

				// UDP hole punching
				stP2PMessage message;
				message.iMessageType = P2PTRASH;
				sendto(PrimaryUDP, (const char *) &message, sizeof(message), 0, (const sockaddr*) &remote, sizeof(remote));

				printf("Send p2ptrash to %s:%ld\n", inet_ntoa(remote.sin_addr), htons(remote.sin_port));
				break;
			}
			case P2PMESSAGEACK: {
				// 发送消息的应答
				RecvedACK = true;
				printf("Recv message ack from %s:%ld\n", inet_ntoa(remote.sin_addr), htons(remote.sin_port));
	 
				break;
			}
			case P2PTRASH: {
				// 对方发送的打洞消息，忽略掉。
				//do nothing ...
				printf("Recv p2ptrash data from %s:%ld\n", inet_ntoa(remote.sin_addr), htons(remote.sin_port));
				break;
			}
			case GETALLUSER: {
				int usercount;
				int fromlen = sizeof(remote);
				int iread = recvfrom(PrimaryUDP, (char *) &usercount, sizeof(int), 0, (sockaddr *) &remote,(socklen_t*) &fromlen);
				if (iread <= 0) {
				  //throw Exception("Login error\n");
				  return NULL;
				}

				ClientList.clear();

				cout << "Have " << usercount << " users logined server:" << endl;
				for (int i = 0; i < usercount; i++) {
					stUserListNode *node = new stUserListNode;
					recvfrom(PrimaryUDP, (char*) node, sizeof(stUserListNode), 0, (sockaddr *) &remote,(socklen_t*) &fromlen);
					ClientList.push_back(node);
					cout << "Username:" << node->userName << endl;
					in_addr tmp;
					tmp.s_addr = htonl(node->ip);
					cout << "UserIP:" << inet_ntoa(tmp) << endl;
					cout << "UserPort:" << node->port << endl;
					cout << "" << endl;
				}
				break;
			}
		}
	}
}

int main(int argc, char* argv[]) {
	try {
		PrimaryUDP = mksock(SOCK_DGRAM);
		BindSock(PrimaryUDP);

		cout << "Please input server ip:";
		cin >> ServerIP;

		cout << "Please input your name:";
		cin >> UserName;

		ConnectToServer(PrimaryUDP, UserName, ServerIP);

		pthread_t thread_id=-1;
		if( pthread_create(&thread_id, NULL, RecvThreadProc, &PrimaryUDP )!=0 ) {
			return -1;
		}
		pthread_detach(thread_id);
		OutputUsage();

		for (;;) {
			char Command[COMMANDMAXC];
			fgets(Command, COMMANDMAXC, stdin);
			ParseCommand(Command);
		}
	} catch (...) {
		return 1;
	}
	return 0;
}

