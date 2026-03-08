#pragma once
#include <ntddk.h>

NTSTATUS DeviceCreate(_In_ PDRIVER_OBJECT DriverObject);

VOID DeviceUnload(_In_ PDRIVER_OBJECT DriverObject);

NTSTATUS DeviceIoControl(
	PDEVICE_OBJECT DeviceObject,
	PIRP Irp
);

NTSTATUS DeviceCreateClose(
	PDEVICE_OBJECT DeviceObject,
	PIRP Irp
);