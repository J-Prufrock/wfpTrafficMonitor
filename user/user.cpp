#include <windows.h>
#include <iostream>
#include <fstream>

#pragma comment(lib, "Ws2_32.lib")

#define IOCTL_SET_PID CTL_CODE(FILE_DEVICE_UNKNOWN,0x800,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define IOCTL_GET_STATS CTL_CODE(FILE_DEVICE_UNKNOWN,0x801,METHOD_BUFFERED,FILE_ANY_ACCESS)
//最多记录DST
#define MAX_ENTRIES 128

typedef struct _FLOW_ENTRY
{
	UINT32 ip;
	UINT16 port;
	UINT64 tx;
	UINT64 rx;
} FLOW_ENTRY;

typedef struct _FLOW_STATS
{
	UINT32 count;
	FLOW_ENTRY entries[MAX_ENTRIES];
} FLOW_STATS;

int main()
{
	STARTUPINFO si{ sizeof(si) };
	PROCESS_INFORMATION pi;

	if (!CreateProcess(L"test.exe", NULL, NULL, NULL, FALSE, CREATE_NO_WINDOW | CREATE_SUSPENDED, NULL, NULL, &si, &pi))
	{
		std::cout << "CreateProcess failed\n";
		return 1;
	}
	std::cout << "CreateProcess success\n";

	HANDLE dev = CreateFile(L"\\\\.\\WfpMonitor", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

	if (dev == INVALID_HANDLE_VALUE)
	{
		std::cout << "Open driver failed\n";
		return 1;
	}
	std::cout << "Open driver success\n";

	DWORD pid = pi.dwProcessId;
	DWORD ret;

	if (DeviceIoControl(dev, IOCTL_SET_PID, &pid, sizeof(pid), NULL, 0, &ret, NULL))
		std::cout << "send PID success,pid:" << pid << "\n";

	if (ResumeThread(pi.hThread) == (DWORD)-1)
	{
		std::cout << "ResumeThread failed\n";
	}

	WaitForSingleObject(pi.hProcess, INFINITE);

	FLOW_STATS stats;
	if (DeviceIoControl(dev, IOCTL_GET_STATS, NULL, 0, &stats, sizeof(stats), &ret, NULL))
		std::cout << "get data success,entries:" << stats.count << '\n';


	std::ofstream f("result.txt");
	for (unsigned int i = 0; i < stats.count; i++)
	{
		stats.entries[i].ip = ntohl(stats.entries[i].ip);
		stats.entries[i].port = ntohs(stats.entries[i].port);
		unsigned char b1 = stats.entries[i].ip & 0xFF;
		unsigned char b2 = (stats.entries[i].ip >> 8) & 0xFF;
		unsigned char b3 = (stats.entries[i].ip >> 16) & 0xFF;
		unsigned char b4 = (stats.entries[i].ip >> 24) & 0xFF;

		f << "DST=" << (int)b1 << "." << (int)b2 << "." << (int)b3 << "." << (int)b4
			<< ":" << stats.entries[i].port << "\n";

		f << "TX_BYTES=" << stats.entries[i].tx << "\n";
		f << "RX_BYTES=" << stats.entries[i].rx << "\n\n";
	}

	f.close();
	CloseHandle(dev);
	return 0;
}