#include <list>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include "common.h"

using namespace std;

unsigned short  g_nServerPort = SERVER_PORT;

UserList ClientList;

int mksock(int type) {
	int sock = socket(AF_INET, type, 0);
	if (sock < 0) {
		printf("create socket error");
	 
	}
	return sock;
}

stUserListNode GetUser(char *username) {
	for (UserList::iterator UserIterator = ClientList.begin(); UserIterator != ClientList.end(); ++UserIterator) {
		if (strcmp(((*UserIterator)->userName), username) == 0)
			return *(*UserIterator);
	}
}
 
int main(int argc, char* argv[]) {
	try {
		int  PrimaryUDP;
		PrimaryUDP = mksock(SOCK_DGRAM);
 
		sockaddr_in local;
		local.sin_family = AF_INET;
		local.sin_port = htons(g_nServerPort);
		local.sin_addr.s_addr = htonl(INADDR_ANY);
		int nResult = bind(PrimaryUDP, (sockaddr*) &local, sizeof(sockaddr));
		if (nResult == -1)
			return -1;
 
		sockaddr_in sender;
		stMessage recvbuf;
		memset(&recvbuf, 0, sizeof(stMessage));

		// 开始主循环.
		// 主循环负责下面几件事情:
		// 一:读取客户端登陆和登出消息,记录客户列表
		// 二:转发客户p2p请求
		for (;;) {
			int dwSender = sizeof(sender);
			int ret = recvfrom(PrimaryUDP, (char *) &recvbuf, sizeof(stMessage), 0, (sockaddr *) &sender, (socklen_t* )&dwSender);
			if (ret <= 0) {
				printf("recv error");
				continue;
			} else {
				int messageType = recvbuf.iMessageType;
				switch (messageType) {
					case LOGIN: {
						//  将这个用户的信息记录到用户列表中
						stUserListNode *currentuser = new stUserListNode();
						strcpy(currentuser->userName,
							recvbuf.message.loginmember.userName);
						currentuser->ip = ntohl(sender.sin_addr.s_addr);
						currentuser->port = ntohs(sender.sin_port);
						currentuser->privateIp = recvbuf.message.loginmember.privateIp;

						in_addr tmp1;
						tmp1.s_addr = htonl(currentuser->privateIp);
						printf( "localIp: %s\n",inet_ntoa( tmp1 ) );

						bool bFound = false;
						for (UserList::iterator UserIterator = ClientList.begin(); UserIterator != ClientList.end(); ++UserIterator) {
							if (strcmp(((*UserIterator)->userName), recvbuf.message.loginmember.userName) == 0) {
								bFound = true;
								break;
							}
						}

						if (!bFound) {
							printf("has a user login : %s <-> %s:%ld\n", recvbuf.message.loginmember.userName, inet_ntoa(sender.sin_addr), ntohs(sender.sin_port));
							ClientList.push_back(currentuser);
						}

						// 发送已经登陆的客户信息
						int nodecount = (int) ClientList.size();
						sendto(PrimaryUDP, (const char*) &nodecount, sizeof(int), 0, (const sockaddr*) &sender, sizeof(sender));
						for (UserList::iterator UserIterator = ClientList.begin(); UserIterator != ClientList.end(); ++UserIterator) {
							sendto(PrimaryUDP, (const char*) (*UserIterator), sizeof(stUserListNode), 0, (const sockaddr*) &sender, sizeof(sender));
						}

						printf("send user list information to: %s <-> %s:%ld\n", recvbuf.message.loginmember.userName, inet_ntoa(sender.sin_addr), ntohs(sender.sin_port));

						break;
					}
					case LOGOUT: {
						// 将此客户信息删除
						printf("has a user logout : %s <-> %s:%ld\n", recvbuf.message.logoutmember.userName, inet_ntoa(sender.sin_addr), ntohs(sender.sin_port));
						UserList::iterator removeiterator = ClientList.end();
						for (UserList::iterator UserIterator = ClientList.begin(); UserIterator != ClientList.end(); ++UserIterator) {
							if (strcmp(((*UserIterator)->userName), recvbuf.message.logoutmember.userName) == 0) {
								removeiterator = UserIterator;
								break;
							}
						}
						if (removeiterator != ClientList.end())
							ClientList.remove(*removeiterator);
						break;
					}
					case P2PTRANS: {
						// 某个客户希望服务端向另外一个客户发送一个打洞消息
						printf("%s:%ld wants to p2p %s\n", inet_ntoa(sender.sin_addr), ntohs(sender.sin_port), recvbuf.message.translatemessage.userName);
						stUserListNode node = GetUser(recvbuf.message.translatemessage.userName);
						sockaddr_in remote;
						remote.sin_family = AF_INET;
						remote.sin_port = htons(node.port);
						remote.sin_addr.s_addr = htonl(node.ip);

						in_addr tmp;
						tmp.s_addr = htonl(node.ip);
 
						stP2PMessage transMessage;
						transMessage.iMessageType = P2PSOMEONEWANTTOCALLYOU;
						transMessage.iStringLen = ntohl(sender.sin_addr.s_addr);
						transMessage.Port = ntohs(sender.sin_port);
 
						sendto(PrimaryUDP, (const char*) &transMessage, sizeof(transMessage), 0, (const sockaddr *) &remote, sizeof(remote));
						char strremote[20];
						strcpy(strremote,inet_ntoa(remote.sin_addr) );
						printf("tell %s <-> %s:%d to send p2ptrans message to: %s:%ld\n", recvbuf.message.translatemessage.userName, strremote, node.port, inet_ntoa(sender.sin_addr), ntohs(sender.sin_port));

						break;
					}
					case GETALLUSER: {
						int command = GETALLUSER;
						sendto(PrimaryUDP, (const char*) &command, sizeof(int), 0, (const sockaddr*) &sender, sizeof(sender));

						int nodecount = (int) ClientList.size();
						sendto(PrimaryUDP, (const char*) &nodecount, sizeof(int), 0, (const sockaddr*) &sender, sizeof(sender));

						for (UserList::iterator UserIterator = ClientList.begin(); UserIterator != ClientList.end(); ++UserIterator) {
							sendto(PrimaryUDP, (const char*) (*UserIterator), sizeof(stUserListNode), 0, (const sockaddr*) &sender, sizeof(sender));
						}
						printf("send user list information to: %s <-> %s:%ld\n", recvbuf.message.loginmember.userName, inet_ntoa(sender.sin_addr), ntohs(sender.sin_port));

						break;
					}
				}
			}
		}
	} catch(...)  {
	  //printf("hehe");
	  return 1;
	}
 
	return 0;
}

