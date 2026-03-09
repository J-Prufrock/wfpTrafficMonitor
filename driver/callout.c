#include "callout.h"

//定义 GUID
DEFINE_GUID(CALLOUT_STREAM_GUID,
	0x12345678, 0x1111, 0x2222, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x11, 0x22);

DEFINE_GUID(CALLOUT_FLOW_GUID,
	0x87654321, 0x3333, 0x4444, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x33, 0x44);

// 全局变量
static HANDLE gEngineHandle = NULL;
UINT32 gCalloutIdStream = 0, gCalloutIdFlow = 0;
UINT64 gFilterIdStream = 0, gFilterIdFlow = 0;

NTSTATUS FlowNotifyFn(FWPS_CALLOUT_NOTIFY_TYPE notifyType, const GUID* filterKey, const FWPS_FILTER* filter)
{
	return STATUS_SUCCESS;
}

NTSTATUS StreamNotifyFn(FWPS_CALLOUT_NOTIFY_TYPE notifyType, const GUID* filterKey, const FWPS_FILTER* filter)
{
	return STATUS_SUCCESS;
}

VOID ALEFlowDeleteFn(UINT16 layerId, UINT32 calloutId, UINT64 flowContext)
{
}

//TRANSPORT层的分类函数，记录流量数据
VOID StreamClassifyFn(const FWPS_INCOMING_VALUES* inFixedValues, const FWPS_INCOMING_METADATA_VALUES* inMetaValues, void* layerData, const void* classifyContext, const FWPS_FILTER* filter, UINT64 flowContext, FWPS_CLASSIFY_OUT* classifyOut)
{
	if (!layerData || !flowContext)
	{
		classifyOut->actionType = FWP_ACTION_PERMIT;
		return;
	}

	PFLOW_CONTEXT ctx = (PFLOW_CONTEXT)flowContext;

	//只处理目标 PID 的流量
	if (!ctx || ctx->magic != 0x12345678 || ctx->pid != gTargetPid)
	{
		classifyOut->actionType = FWP_ACTION_PERMIT;
		return;
	}

	FWPS_STREAM_DATA* streamData = (FWPS_STREAM_DATA*)layerData;

	UINT64 bytes = streamData->dataLength;

	if (streamData->flags & FWPS_STREAM_FLAG_RECEIVE)
		ctx->bytesRecv += bytes;
	else
		ctx->bytesSent += bytes;

	classifyOut->actionType = FWP_ACTION_PERMIT;
}

//创建flowContext并将其与flow关联
VOID FlowClassifyFn(const FWPS_INCOMING_VALUES* inFixedValues, const FWPS_INCOMING_METADATA_VALUES* inMetaValues, void* layerData, const void* classifyContext, const FWPS_FILTER* filter, UINT64 flowContext, FWPS_CLASSIFY_OUT* classifyOut)
{
	//创建flowContext
	PFLOW_CONTEXT ctx = ExAllocatePoolWithTag(NonPagedPool, sizeof(FLOW_CONTEXT), 'wfp1');

	if (!ctx)
	{
		classifyOut->actionType = FWP_ACTION_PERMIT;
		return;
	}

	RtlZeroMemory(ctx, sizeof(FLOW_CONTEXT));

	ctx->magic = 0x12345678;

	//记录进程ID
	ctx->pid = inMetaValues->processId;

	//记录IP和端口信息
	ctx->localIp = inFixedValues->incomingValue[FWPS_FIELD_ALE_FLOW_ESTABLISHED_V4_IP_LOCAL_ADDRESS].value.uint32;
	ctx->remoteIp = inFixedValues->incomingValue[FWPS_FIELD_ALE_FLOW_ESTABLISHED_V4_IP_REMOTE_ADDRESS].value.uint32;
	ctx->localPort = inFixedValues->incomingValue[FWPS_FIELD_ALE_FLOW_ESTABLISHED_V4_IP_LOCAL_PORT].value.uint16;
	ctx->remotePort = inFixedValues->incomingValue[FWPS_FIELD_ALE_FLOW_ESTABLISHED_V4_IP_REMOTE_PORT].value.uint16;

	//将flowContext与flow关联
	FwpsFlowAssociateContext(inMetaValues->flowHandle, FWPS_LAYER_STREAM_V4, gCalloutIdStream, (UINT64)ctx);

	classifyOut->actionType = FWP_ACTION_PERMIT;
}


//删除flowContext并将其中的数据添加到gStats中
VOID FlowDeleteFn(UINT16 layerId, UINT32 calloutId, UINT64 flowContext)
{
	if (!flowContext)
		return;

	PFLOW_CONTEXT ctx = (PFLOW_CONTEXT)flowContext;

	if (!ctx || ctx->magic != 0x12345678)
	{
		ExFreePool(ctx);
		return;
	}

	//只记录目标 PID
	if (ctx->pid != gTargetPid)
	{
		ExFreePool(ctx);
		return;
	}

	//加锁保护 gStats
	KIRQL oldIrql;
	KeAcquireSpinLock(&gStatsLock, &oldIrql);

	//合并到原有记录
	for (UINT32 i = 0; i < gStats.count; i++)
	{
		if (gStats.entries[i].ip == ctx->remoteIp && gStats.entries[i].port == ctx->remotePort)
		{
			gStats.entries[i].tx += ctx->bytesSent;
			gStats.entries[i].rx += ctx->bytesRecv;
			goto EXIT;
		}
	}

	//添加到新纪录
	if (gStats.count < MAX_ENTRIES)
	{
		UINT32 i = gStats.count;

		gStats.entries[i].ip = ctx->remoteIp;
		gStats.entries[i].port = ctx->remotePort;
		gStats.entries[i].tx = ctx->bytesSent;
		gStats.entries[i].rx = ctx->bytesRecv;

		gStats.count++;
	}

EXIT:
	KeReleaseSpinLock(&gStatsLock, oldIrql);

	ExFreePool(ctx);
}

//wfp的初始化，包括engine的打开、callout的注册和添加以及filter的注册等
NTSTATUS InitialWfp(PDEVICE_OBJECT device)
{
	if (!device)
	{
		DbgPrint("[WfpMonitor] InitialWfp: device is NULL\n");
		return STATUS_INVALID_PARAMETER;
	}

	NTSTATUS status;

	FWPS_CALLOUT sCalloutStream = { 0 };
	FWPS_CALLOUT sCalloutFlow = { 0 };

	FWPM_CALLOUT mCallout = { 0 };
	FWPM_FILTER filter = { 0 };

	//注册 callout

	sCalloutStream.calloutKey = CALLOUT_STREAM_GUID;
	sCalloutStream.classifyFn = StreamClassifyFn;
	sCalloutStream.flowDeleteFn = FlowDeleteFn;
	sCalloutStream.notifyFn = StreamNotifyFn;

	status = FwpsCalloutRegister(device, &sCalloutStream, &gCalloutIdStream);
	if (!NT_SUCCESS(status)) return status;

	//ALE 层 callout
	sCalloutFlow.calloutKey = CALLOUT_FLOW_GUID;
	sCalloutFlow.classifyFn = FlowClassifyFn;
	sCalloutFlow.notifyFn = FlowNotifyFn;
	sCalloutFlow.flowDeleteFn = ALEFlowDeleteFn;

	status = FwpsCalloutRegister(device, &sCalloutFlow, &gCalloutIdFlow);
	if (!NT_SUCCESS(status)) return status;

	//打开 engine
	status = FwpmEngineOpen(NULL, RPC_C_AUTHN_WINNT, NULL, NULL, &gEngineHandle);
	if (!NT_SUCCESS(status)) return status;

	//开始 transaction
	status = FwpmTransactionBegin(gEngineHandle, 0);
	if (!NT_SUCCESS(status)) return status;

	//添加 callout 到 STREAM 层
	RtlZeroMemory(&mCallout, sizeof(mCallout));

	mCallout.calloutKey = CALLOUT_STREAM_GUID;
	mCallout.applicableLayer = FWPM_LAYER_STREAM_V4;
	mCallout.displayData.name = L"WfpMonitor Stream Callout";
	mCallout.displayData.description = L"WfpMonitor Stream Callout";

	status = FwpmCalloutAdd(gEngineHandle, &mCallout, NULL, NULL);
	if (!NT_SUCCESS(status)) goto EXIT;

	//添加 callout 到 ale 层
	RtlZeroMemory(&mCallout, sizeof(mCallout));

	mCallout.calloutKey = CALLOUT_FLOW_GUID;
	mCallout.applicableLayer = FWPM_LAYER_ALE_FLOW_ESTABLISHED_V4;
	mCallout.displayData.name = L"WfpMonitor Flow Callout";
	mCallout.displayData.description = L"Flow monitor";

	status = FwpmCalloutAdd(gEngineHandle, &mCallout, NULL, NULL);
	if (!NT_SUCCESS(status)) goto EXIT;

	//添加 filter 到 STREAM 层
	RtlZeroMemory(&filter, sizeof(filter));

	filter.layerKey = FWPM_LAYER_STREAM_V4;
	filter.subLayerKey = FWPM_SUBLAYER_UNIVERSAL;
	filter.weight.type = FWP_EMPTY;

	filter.action.type = FWP_ACTION_CALLOUT_INSPECTION;
	filter.action.calloutKey = CALLOUT_STREAM_GUID;

	filter.displayData.name = L"WfpMonitor STREAM Filter";
	filter.displayData.description = L"WfpMonitor STREAM Filter";

	status = FwpmFilterAdd(gEngineHandle, &filter, NULL, &gFilterIdStream);
	if (!NT_SUCCESS(status)) goto EXIT;

	//添加 filter 到 ale 层
	RtlZeroMemory(&filter, sizeof(filter));

	filter.layerKey = FWPM_LAYER_ALE_FLOW_ESTABLISHED_V4;
	filter.subLayerKey = FWPM_SUBLAYER_UNIVERSAL;
	filter.weight.type = FWP_EMPTY;

	filter.action.type = FWP_ACTION_CALLOUT_INSPECTION;
	filter.action.calloutKey = CALLOUT_FLOW_GUID;

	filter.displayData.name = L"WfpMonitor Flow Filter";
	filter.displayData.description = L"TCP Flow Monitor";

	status = FwpmFilterAdd(gEngineHandle, &filter, NULL, &gFilterIdFlow);

EXIT:

	if (NT_SUCCESS(status))
		FwpmTransactionCommit(gEngineHandle);
	else
		FwpmTransactionAbort(gEngineHandle);

	return status;
}

// 卸载 WFP
VOID UnInitialWfp()
{
	if (!gEngineHandle)
		return;

	if (gFilterIdStream)
	{
		FwpmFilterDeleteById(gEngineHandle, gFilterIdStream);
		gFilterIdStream = 0;
	}

	if (gFilterIdFlow)
	{
		FwpmFilterDeleteById(gEngineHandle, gFilterIdFlow);
		gFilterIdFlow = 0;
	}

	FwpmCalloutDeleteByKey(gEngineHandle, &CALLOUT_STREAM_GUID);
	FwpmCalloutDeleteByKey(gEngineHandle, &CALLOUT_FLOW_GUID);

	FwpmEngineClose(gEngineHandle);
	gEngineHandle = NULL;

	if (gCalloutIdStream)
	{
		FwpsCalloutUnregisterById(gCalloutIdStream);
		gCalloutIdStream = 0;
	}

	if (gCalloutIdFlow)
	{
		FwpsCalloutUnregisterById(gCalloutIdFlow);
		gCalloutIdFlow = 0;
	}
}
