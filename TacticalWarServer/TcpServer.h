#pragma once

#include <WS2tcpip.h>
#include <exception>
#include "ParserEventListener.h"

#pragma comment (lib, "ws2_32.lib")

#define BUF_SIZE 4096

template<class T, class CS>
class TcpServer : public ParserEventListener
{
private:
	SOCKET listeningSocket;
	fd_set master;
	T * parser;

	bool running;
	HANDLE serverThreadHandle;

	static DWORD WINAPI ServerThread(LPVOID parameters);

public:
	TcpServer(u_short listeningPort);
	~TcpServer();

	static void Send(CS * client, char * buffer, int length);
	void kick(SOCKET s);

	T * getParser();

	virtual void onClientKicked(SOCKET sock);
};

template<class T, class CS>
T * TcpServer<T, CS>::getParser()
{
	return parser;
}

template<class T, class CS>
TcpServer<T, CS>::TcpServer(u_short listeningPort)
{
	WSADATA wsData;
	WORD ver = MAKEWORD(2, 2);

	int wsOk = WSAStartup(ver, &wsData);
	if (wsOk != 0)
	{
		throw std::exception("WSAStartup failed");
	}

	listeningSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (listeningSocket == INVALID_SOCKET)
	{
		WSACleanup();
		throw std::exception("Can't create a socket");
	}

	sockaddr_in hint;
	hint.sin_family = AF_INET;
	hint.sin_port = htons(listeningPort);
	hint.sin_addr.S_un.S_addr = INADDR_ANY;

	bind(listeningSocket, (sockaddr*)&hint, sizeof(hint));

	listen(listeningSocket, SOMAXCONN);

	FD_ZERO(&master);
	FD_SET(listeningSocket, &master);

	parser = new T();
	parser->addEventListener(this);
	running = true;

	DWORD threadId;
	serverThreadHandle = CreateThread(
							NULL,
							0,
							TcpServer<T, CS>::ServerThread,
							this,
							0,
							&threadId);
}

template<class T, class CS>
DWORD WINAPI TcpServer<T, CS>::ServerThread(LPVOID parameters)
{
	TcpServer<T, CS> * server = (TcpServer<T, CS>*)parameters;
	
	timeval waitingTime;
	waitingTime.tv_sec = 0;
	waitingTime.tv_usec = 100;

	while (server->running)
	{
		fd_set copy = server->master;

		int socketCount = select(0, &copy, nullptr, nullptr, &waitingTime);

		for (int i = 0; i < socketCount; i++)
		{
			SOCKET sock = copy.fd_array[i];

			if (sock == server->listeningSocket)
			{
				SOCKET client = accept(server->listeningSocket, nullptr, nullptr);

				FD_SET(client, &(server->master));

				CS * clientState = new CS(client);
				server->parser->onClientConnected(clientState);
			}
			else
			{
				char buf[BUF_SIZE];
				ZeroMemory(buf, BUF_SIZE);

				int bytesIn = recv(sock, buf, BUF_SIZE, 0);
				if (bytesIn <= 0)
				{
					server->kick(sock);
				}
				else
				{
					server->parser->parse(sock, (unsigned char *)buf, bytesIn);
				}
			}
		}
	}

	return 0;
}

template<class T, class CS>
TcpServer<T, CS>::~TcpServer()
{
	running = false;
	WaitForSingleObject(serverThreadHandle, INFINITE);
	FD_CLR(listeningSocket, &master);
	closesocket(listeningSocket);

	while (master.fd_count > 0)
	{
		SOCKET sock = master.fd_array[0];
		FD_CLR(sock, &master);
		closesocket(sock);
	}

	WSACleanup();
	delete parser;
}

template<class T, class CS>
void TcpServer<T, CS>::Send(CS * client, char * buffer, int length)
{
	SOCKET sock = client->getSocket();
	send(sock, buffer, length, 0);
}

template<class T, class CS>
void TcpServer<T, CS>::onClientKicked(SOCKET sock)
{
	kick(sock);
}

template<class T, class CS>
void TcpServer<T, CS>::kick(SOCKET s)
{
	closesocket(s);
	FD_CLR(s, &(master));
	parser->onClientDisconnected(s);
}