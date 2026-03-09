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
	addr.sin_port = htons(8080);
	inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

	bind(listener, (sockaddr*)&addr, sizeof(addr));
	listen(listener, 5);

	std::cout << "Server listening\n";

	SOCKET client = accept(listener, NULL, NULL);

	char buf[4096];

	int n;
	long long total = 0;

	while ((n = recv(client, buf, sizeof(buf), 0)) > 0)
	{
		total += n;
	}

	std::cout << "Server received bytes: " << total << std::endl;

	closesocket(client);
	closesocket(listener);
	WSACleanup();
}

void run_client()
{
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

	std::cout << "Client connected\n";

	const int SIZE = 1024 * 1024;   // 1MB
	char* buffer = new char[SIZE];

	memset(buffer, 'A', SIZE);

	int total = 0;

	while (total < SIZE)
	{
		int sent = send(s, buffer + total, SIZE - total, 0);
		if (sent <= 0) break;
		total += sent;
	}

	std::cout << "Client sent bytes: " << total << std::endl;

	delete[] buffer;

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