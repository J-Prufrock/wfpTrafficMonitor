#pragma once
//设置NDIS版本号
#define NDIS620

#include <ntddk.h>
#include <fwpsk.h>
#define INITGUID
#include <guiddef.h>
#include <fwpmk.h>

//IO通信所用的设备和符号链接名称
#define DEVICE_NAME L"\\Device\\WfpMonitor"
#define SYMLINK_NAME L"\\DosDevices\\WfpMonitor"

//定义IOCTL码用于通信
#define IOCTL_SET_PID CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_GET_STATS CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

//IO通信结构体中的记录数量
#define MAX_ENTRIES 128

//flowContext结构体
typedef struct _FLOW_CONTEXT {
	UINT32 magic;       // 标记
	UINT64 pid;         // 进程ID
	UINT64 bytesSent;
	UINT64 bytesRecv;
	UINT32 localIp;
	UINT32 remoteIp;
	UINT16 localPort;
	UINT16 remotePort;
} FLOW_CONTEXT, * PFLOW_CONTEXT;

//IO结构体单元
typedef struct _FLOW_ENTRY
{
	UINT32 ip;
	UINT16 port;
	UINT64 tx;
	UINT64 rx;
} FLOW_ENTRY;

//IO结构体
typedef struct _FLOW_STATS
{
	UINT32 count;
	FLOW_ENTRY entries[MAX_ENTRIES];
} FLOW_STATS, * PFLOW_STATS;

// 声明外部 GUID
extern const GUID calloutKey;
extern const GUID sublayerKey;
//声明 目标进程pid,IO结构体和锁
extern UINT32 gTargetPid;
extern FLOW_STATS gStats;
extern KSPIN_LOCK gStatsLock;
