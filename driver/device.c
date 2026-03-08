#include "driver.h"
#include "device.h"

NTSTATUS DeviceCreateClose(
	PDEVICE_OBJECT DeviceObject,
	PIRP Irp
)
{
	UNREFERENCED_PARAMETER(DeviceObject);

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;

	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}
//
// Create / Close 处理
//
NTSTATUS DeviceCreate(_In_ PDRIVER_OBJECT DriverObject)
{
	//UNREFERENCED_PARAMETER(DriverObject);

	NTSTATUS status;
	PDEVICE_OBJECT deviceObject = NULL;
	UNICODE_STRING deviceName;
	UNICODE_STRING symLink;

	// 初始化设备名
	RtlInitUnicodeString(&deviceName, DEVICE_NAME);

	// 创建设备
	status = IoCreateDevice(
		DriverObject,
		0,
		&deviceName,
		FILE_DEVICE_UNKNOWN,
		0,
		FALSE,
		&deviceObject
	);

	if (!NT_SUCCESS(status)) {
		DbgPrint("[WfpMonitor]DeviceCreate: IoCreateDevice failed %X\n", status);
		return status;
	}

	deviceObject->Flags |= DO_BUFFERED_IO;
	deviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

	// 创建符号链接
	RtlInitUnicodeString(&symLink, SYMLINK_NAME);
	status = IoCreateSymbolicLink(&symLink, &deviceName);
	if (!NT_SUCCESS(status)) {
		IoDeleteDevice(deviceObject);
		DbgPrint("[WfpMonitor]DeviceCreate: IoCreateSymbolicLink failed %X\n", status);
		return status;
	}

	// 设置驱动函数
	DriverObject->MajorFunction[IRP_MJ_CREATE] = DeviceCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = DeviceCreateClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DeviceIoControl;

	DbgPrint("[WfpMonitor]DeviceCreate: Device and symbolic link created.\n");
	return STATUS_SUCCESS;
}

//
// 卸载设备
//
VOID DeviceUnload(_In_ PDRIVER_OBJECT DriverObject)
{
	UNICODE_STRING symLink;

	RtlInitUnicodeString(&symLink, SYMLINK_NAME);
	IoDeleteSymbolicLink(&symLink);

	if (DriverObject->DeviceObject) {
		IoDeleteDevice(DriverObject->DeviceObject);
	}

	DbgPrint("[WfpMonitor]DeviceUnload: Device and symbolic link deleted.\n");
}

//
// IOCTL 处理
//
NTSTATUS DeviceIoControl(
	PDEVICE_OBJECT DeviceObject,
	PIRP Irp
)
{
	//UNREFERENCED_PARAMETER(DeviceObject);

	NTSTATUS status = STATUS_SUCCESS;
	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
	ULONG code = stack->Parameters.DeviceIoControl.IoControlCode;

	switch (code) {
	case IOCTL_SET_PID:
		if (stack->Parameters.DeviceIoControl.InputBufferLength < sizeof(UINT32)) {
			status = STATUS_BUFFER_TOO_SMALL;
			break;
		}
		gTargetPid = *(UINT32*)Irp->AssociatedIrp.SystemBuffer;
		DbgPrint("[WfpMonitor]IOCTL_SET_PID: Target PID = %u\n", gTargetPid);
		Irp->IoStatus.Information = 0;
		break;

	case IOCTL_GET_STATS:
	{
		if (stack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(FLOW_STATS))
		{
			status = STATUS_BUFFER_TOO_SMALL;
			break;
		}

		KIRQL oldIrql;
		KeAcquireSpinLock(&gStatsLock, &oldIrql);

		RtlCopyMemory(
			Irp->AssociatedIrp.SystemBuffer,
			&gStats,
			sizeof(FLOW_STATS)
		);

		KeReleaseSpinLock(&gStatsLock, oldIrql);

		Irp->IoStatus.Information = sizeof(FLOW_STATS);
		break;
	}

	default:
		status = STATUS_INVALID_DEVICE_REQUEST;
		break;
	}

	Irp->IoStatus.Status = status;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}