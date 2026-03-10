#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <iostream>
#include <cstring>

#pragma comment(lib, "Ws2_32.lib")

// 服务端函数
void run_server(int listen_port)
{
	WSADATA wsa;
	WSAStartup(MAKEWORD(2, 2), &wsa);

	SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(listen_port);
	inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

	bind(listener, (sockaddr*)&addr, sizeof(addr));
	listen(listener, 5);

	std::cout << "Server listening on port " << listen_port << "\n";

	SOCKET client = accept(listener, NULL, NULL);
	std::cout << "Server accepted connection on port " << listen_port << "\n";

	// 接收客户端发送的 1024 字节
	char recv_buf[1024];
	int n = 0;
	int total = 0;
	while (total < 1024 && (n = recv(client, recv_buf + total, 1024 - total, 0)) > 0)
		total += n;

	std::cout << "Server received " << total << " bytes from client\n";

	// 回复客户端 2048 字节
	char reply[2048];
	memset(reply, 'S', sizeof(reply));
	total = 0;
	while (total < 2048)
	{
		n = send(client, reply + total, 2048 - total, 0);
		if (n <= 0) break;
		total += n;
	}
	std::cout << "Server sent " << total << " bytes to client\n";

	closesocket(client);
	closesocket(listener);
	WSACleanup();
}

// 客户端函数
void run_client(int src_port, int dst_port)
{
	Sleep(200); // 等服务端起来

	WSADATA wsa;
	WSAStartup(MAKEWORD(2, 2), &wsa);

	SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	// 绑定客户端源端口
	sockaddr_in src_addr{};
	src_addr.sin_family = AF_INET;
	src_addr.sin_port = htons(src_port);
	inet_pton(AF_INET, "127.0.0.1", &src_addr.sin_addr);

	bind(s, (sockaddr*)&src_addr, sizeof(src_addr));

	// 连接服务端
	sockaddr_in server{};
	server.sin_family = AF_INET;
	server.sin_port = htons(dst_port);
	inet_pton(AF_INET, "127.0.0.1", &server.sin_addr);

	if (connect(s, (sockaddr*)&server, sizeof(server)) == SOCKET_ERROR)
	{
		std::cout << "Client failed to connect from port " << src_port << " to " << dst_port << "\n";
		closesocket(s);
		WSACleanup();
		return;
	}

	std::cout << "Client connected from port " << src_port << " to " << dst_port << "\n";

	// 发送 1024 字节
	char send_buf[1024];
	memset(send_buf, 'C', sizeof(send_buf));
	int total = 0;
	int n = 0;
	while (total < 1024)
	{
		n = send(s, send_buf + total, 1024 - total, 0);
		if (n <= 0) break;
		total += n;
	}
	std::cout << "Client sent " << total << " bytes\n";

	// 接收服务端回复 2048 字节
	char recv_buf[2048];
	total = 0;
	while (total < 2048 && (n = recv(s, recv_buf + total, 2048 - total, 0)) > 0)
		total += n;

	std::cout << "Client received " << total << " bytes reply\n";

	closesocket(s);
	WSACleanup();
}

int main()
{
	// 第一次连接: 客户端50101 -> 服务端18080
	std::thread server1(run_server, 18080);
	std::thread client1(run_client, 50101, 18080);

	client1.join();
	server1.join();

	// 第二次连接: 客户端50102 -> 服务端18081
	std::thread server2(run_server, 18081);
	std::thread client2(run_client, 50102, 18081);

	client2.join();
	server2.join();

	std::thread server3(run_server, 18082);
	std::thread client3(run_client, 50103, 18082);

	client3.join();
	server3.join();

	std::thread server4(run_server, 18083);
	std::thread client4(run_client, 50104, 18083);

	client4.join();
	server4.join();

	std::cout << "All connections completed.\n";
	return 0;
}