#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <iostream>

#pragma comment(lib, "Ws2_32.lib")

void run_server()
{
	WSADATA wsa;
	WSAStartup(MAKEWORD(2, 2), &wsa);

	SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(8080);           // 服务端监听端口
	inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

	bind(listener, (sockaddr*)&addr, sizeof(addr));
	listen(listener, 5);
	std::cout << "Server listening on 127.0.0.1:8080\n";

	SOCKET client = accept(listener, NULL, NULL);
	char buf[1024];
	int n;
	while ((n = recv(client, buf, sizeof(buf), 0)) > 0)
	{
		// 回显数据
		send(client, buf, n, 0);
	}

	closesocket(client);
	closesocket(listener);
	WSACleanup();
}

void run_client()
{
	// 等服务器启动
	Sleep(500);

	WSADATA wsa;
	WSAStartup(MAKEWORD(2, 2), &wsa);

	SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	sockaddr_in server;
	server.sin_family = AF_INET;
	server.sin_port = htons(8080);
	inet_pton(AF_INET, "127.0.0.1", &server.sin_addr);

	if (connect(s, (sockaddr*)&server, sizeof(server)) == SOCKET_ERROR)
	{
		std::cout << "Connect failed\n";
		closesocket(s);
		WSACleanup();
		return;
	}

	std::cout << "Client connected to server\n";

	const char* msg = "Hello, WFP monitor!\n";
	for (int i = 0; i < 10; i++)
	{
		send(s, msg, strlen(msg), 0);

		// 接收回显
		char buf[1024];
		int n = recv(s, buf, sizeof(buf) - 1, 0);
		if (n > 0)
		{
			buf[n] = 0;
			std::cout << "Received: " << buf;
		}
	}

	closesocket(s);
	WSACleanup();
}

int main()
{
	// 服务端线程
	std::thread server_thread(run_server);

	// 客户端线程
	std::thread client_thread(run_client);

	client_thread.join();
	server_thread.join();

	return 0;
}