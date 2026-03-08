#include "driver.h"
#include "device.h"
#include "callout.h"

//定义目标进程pid和IO通信结构体
UINT32 gTargetPid = 0;
FLOW_STATS gStats = { 0 };

//定义锁变量，用于IO通信结构体的并发访问
KSPIN_LOCK gStatsLock;

// 定义 GUID


//卸载驱动
VOID DriverUnload(PDRIVER_OBJECT DriverObject)
{
	//卸载wfp
	UnInitialWfp();
	//删除设备
	DeviceUnload(DriverObject);
}

//驱动入口
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	DbgPrint("[wfpMonitor] enter the driver \n");

	//初始化锁
	KeInitializeSpinLock(&gStatsLock);

	//绑定卸载函数
	DriverObject->DriverUnload = DriverUnload;

	NTSTATUS status;

	//创建设备对象
	status = DeviceCreate(DriverObject);

	if (!NT_SUCCESS(status))
		return status;

	//初始化WFP
	status = InitialWfp(DriverObject->DeviceObject);

	return status;
}