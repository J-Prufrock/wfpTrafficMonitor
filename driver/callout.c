
#include "callout.h"

//定义 GUID
DEFINE_GUID(CALLOUT_OUTBOUND_GUID,
	0x12345678, 0x1111, 0x2222, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x11, 0x22);

DEFINE_GUID(CALLOUT_INBOUND_GUID,
	0x12345679, 0x1111, 0x2222, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x11, 0x23);

DEFINE_GUID(CALLOUT_FLOW_GUID,
	0x87654321, 0x3333, 0x4444, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x33, 0x44);

// 全局变量
static HANDLE gEngineHandle = NULL;
UINT32 gCalloutIdOutbound = 0, gCalloutIdInbound = 0, gCalloutIdFlow = 0;
UINT64 gFilterIdOutbound = 0, gFilterIdInbound = 0, gFilterIdFlow = 0;

NTSTATUS FlowNotifyFn(FWPS_CALLOUT_NOTIFY_TYPE notifyType, const GUID* filterKey, const FWPS_FILTER* filter)
{
	return STATUS_SUCCESS;
}

NTSTATUS TransportNotifyFn(FWPS_CALLOUT_NOTIFY_TYPE notifyType, const GUID* filterKey, const FWPS_FILTER* filter)
{
	return STATUS_SUCCESS;
}

VOID ALEFlowDeleteFn(UINT16 layerId, UINT32 calloutId, UINT64 flowContext)
{
}

//TRANSPORT层的分类函数，记录流量数据
VOID TransportClassifyFn(const FWPS_INCOMING_VALUES* inFixedValues, const FWPS_INCOMING_METADATA_VALUES* inMetaValues, void* layerData, const void* classifyContext, const FWPS_FILTER* filter, UINT64 flowContext, FWPS_CLASSIFY_OUT* classifyOut)
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

	//统计字节数
	NET_BUFFER_LIST* nbl = (NET_BUFFER_LIST*)layerData;

	UINT64 bytes = 0;

	for (; nbl; nbl = NET_BUFFER_LIST_NEXT_NBL(nbl))
	{
		NET_BUFFER* nb = NET_BUFFER_LIST_FIRST_NB(nbl);

		for (; nb; nb = NET_BUFFER_NEXT_NB(nb))
		{
			bytes += NET_BUFFER_DATA_LENGTH(nb);
		}
	}
	if (inFixedValues == 0)
	{
		classifyOut->actionType = FWP_ACTION_PERMIT;
		return;
	}

	if (inFixedValues->layerId == FWPS_LAYER_OUTBOUND_TRANSPORT_V4)
		ctx->bytesSent += bytes;
	else
		ctx->bytesRecv += bytes;

	classifyOut->actionType = FWP_ACTION_PERMIT;
}

//创建flowContext并将其与flow关联
VOID FlowClassifyFn(const FWPS_INCOMING_VALUES* inFixedValues, const FWPS_INCOMING_METADATA_VALUES* inMetaValues, void* layerData, const void* classifyContext, const FWPS_FILTER* filter, UINT64 flowContext, FWPS_CLASSIFY_OUT* classifyOut)
{
	//创建flowContext
	PFLOW_CONTEXT ctx1 = ExAllocatePoolWithTag(NonPagedPool, sizeof(FLOW_CONTEXT), 'wfp1');
	PFLOW_CONTEXT ctx2 = ExAllocatePoolWithTag(NonPagedPool, sizeof(FLOW_CONTEXT), 'wfp2');

	if (!ctx1 || !ctx2)
	{
		if (ctx1) ExFreePool(ctx1);
		if (ctx2) ExFreePool(ctx2);

		classifyOut->actionType = FWP_ACTION_PERMIT;
		return;
	}

	RtlZeroMemory(ctx1, sizeof(FLOW_CONTEXT));
	RtlZeroMemory(ctx2, sizeof(FLOW_CONTEXT));


	ctx1->magic = 0x12345678;
	//记录进程ID
	ctx1->pid = inMetaValues->processId;
	//记录IP和端口信息
	ctx1->localIp = inFixedValues->incomingValue[FWPS_FIELD_ALE_FLOW_ESTABLISHED_V4_IP_LOCAL_ADDRESS].value.uint32;
	ctx1->remoteIp = inFixedValues->incomingValue[FWPS_FIELD_ALE_FLOW_ESTABLISHED_V4_IP_REMOTE_ADDRESS].value.uint32;
	ctx1->localPort = inFixedValues->incomingValue[FWPS_FIELD_ALE_FLOW_ESTABLISHED_V4_IP_LOCAL_PORT].value.uint16;
	ctx1->remotePort = inFixedValues->incomingValue[FWPS_FIELD_ALE_FLOW_ESTABLISHED_V4_IP_REMOTE_PORT].value.uint16;

	ctx2->magic = 0x12345678;
	//记录进程ID
	ctx2->pid = inMetaValues->processId;
	//记录IP和端口信息
	ctx2->localIp = inFixedValues->incomingValue[FWPS_FIELD_ALE_FLOW_ESTABLISHED_V4_IP_LOCAL_ADDRESS].value.uint32;
	ctx2->remoteIp = inFixedValues->incomingValue[FWPS_FIELD_ALE_FLOW_ESTABLISHED_V4_IP_REMOTE_ADDRESS].value.uint32;
	ctx2->localPort = inFixedValues->incomingValue[FWPS_FIELD_ALE_FLOW_ESTABLISHED_V4_IP_LOCAL_PORT].value.uint16;
	ctx2->remotePort = inFixedValues->incomingValue[FWPS_FIELD_ALE_FLOW_ESTABLISHED_V4_IP_REMOTE_PORT].value.uint16;

	//将flowContext与flow关联
	FwpsFlowAssociateContext(inMetaValues->flowHandle, FWPS_LAYER_OUTBOUND_TRANSPORT_V4, gCalloutIdOutbound, (UINT64)ctx1);

	FwpsFlowAssociateContext(inMetaValues->flowHandle, FWPS_LAYER_INBOUND_TRANSPORT_V4, gCalloutIdInbound, (UINT64)ctx2);

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
		gStats.entries[i].tx += ctx->bytesSent;
		gStats.entries[i].rx += ctx->bytesRecv;

		gStats.count++;
	}

EXIT:
	//释放锁
	KeReleaseSpinLock(&gStatsLock, oldIrql);
	//释放 flowContext 内存
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
	FWPS_CALLOUT sCalloutOutbound = { 0 };
	FWPS_CALLOUT sCalloutInbound = { 0 };
	FWPS_CALLOUT sCalloutFlow = { 0 };
	FWPM_CALLOUT mCallout = { 0 };
	FWPM_FILTER filter = { 0 };

	//注册 callout
	// OUTBOUND callout
	sCalloutOutbound.calloutKey = CALLOUT_OUTBOUND_GUID;
	sCalloutOutbound.classifyFn = TransportClassifyFn;
	sCalloutOutbound.flowDeleteFn = FlowDeleteFn;
	sCalloutOutbound.notifyFn = TransportNotifyFn;


	status = FwpsCalloutRegister(device, &sCalloutOutbound, &gCalloutIdOutbound);
	if (!NT_SUCCESS(status)) return status;

	//INBOUND callout
	sCalloutInbound.calloutKey = CALLOUT_INBOUND_GUID;
	sCalloutInbound.classifyFn = TransportClassifyFn;
	sCalloutInbound.flowDeleteFn = FlowDeleteFn;
	sCalloutInbound.notifyFn = TransportNotifyFn;

	status = FwpsCalloutRegister(device, &sCalloutInbound, &gCalloutIdInbound);
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

	//添加 callout 到 OUTBOUND 层和INBOUND层
	RtlZeroMemory(&mCallout, sizeof(mCallout));
	mCallout.calloutKey = CALLOUT_OUTBOUND_GUID;
	mCallout.applicableLayer = FWPM_LAYER_OUTBOUND_TRANSPORT_V4;
	mCallout.displayData.name = L"WfpMonitor OutBound Callout";
	mCallout.displayData.description = L"WfpMonitor OutBound Callout";
	status = FwpmCalloutAdd(gEngineHandle, &mCallout, NULL, NULL);
	if (!NT_SUCCESS(status)) goto EXIT;

	RtlZeroMemory(&mCallout, sizeof(mCallout));
	mCallout.calloutKey = CALLOUT_INBOUND_GUID;
	mCallout.applicableLayer = FWPM_LAYER_INBOUND_TRANSPORT_V4;
	mCallout.displayData.name = L"WfpMonitor InBound Callout";
	mCallout.displayData.description = L"WfpMonitor InBound Callout";
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

	//添加 filter 到 OUTBOUND 层和INBOUND层
	RtlZeroMemory(&filter, sizeof(filter));
	filter.layerKey = FWPM_LAYER_OUTBOUND_TRANSPORT_V4;
	filter.subLayerKey = FWPM_SUBLAYER_UNIVERSAL;
	filter.weight.type = FWP_EMPTY;
	filter.action.type = FWP_ACTION_CALLOUT_INSPECTION;
	filter.action.calloutKey = CALLOUT_OUTBOUND_GUID;
	filter.displayData.name = L"WfpMonitor OUTBOUND Filter";
	filter.displayData.description = L"WfpMonitor OUTBOUND Filter";
	status = FwpmFilterAdd(gEngineHandle, &filter, NULL, &gFilterIdOutbound);
	if (!NT_SUCCESS(status)) goto EXIT;

	RtlZeroMemory(&filter, sizeof(filter));
	filter.layerKey = FWPM_LAYER_INBOUND_TRANSPORT_V4;
	filter.subLayerKey = FWPM_SUBLAYER_UNIVERSAL;
	filter.weight.type = FWP_EMPTY;
	filter.action.type = FWP_ACTION_CALLOUT_INSPECTION;
	filter.action.calloutKey = CALLOUT_INBOUND_GUID;
	filter.displayData.name = L"WfpMonitor INBOUND Filter";
	filter.displayData.description = L"WfpMonitor INBOUND Filter";
	status = FwpmFilterAdd(gEngineHandle, &filter, NULL, &gFilterIdInbound);
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
	//提交或回滚 transaction
	if (NT_SUCCESS(status))
		FwpmTransactionCommit(gEngineHandle);
	else
		FwpmTransactionAbort(gEngineHandle);

	return status;
}

// 卸载 WFP
VOID UnInitialWfp()
{
	NTSTATUS status;

	if (!gEngineHandle)
		return;

	//删除 filter
	if (gFilterIdInbound)
	{
		FwpmFilterDeleteById(gEngineHandle, gFilterIdInbound);
		gFilterIdInbound = 0;
	}

	if (gFilterIdOutbound)
	{
		FwpmFilterDeleteById(gEngineHandle, gFilterIdOutbound);
		gFilterIdOutbound = 0;
	}

	if (gFilterIdFlow)
	{
		FwpmFilterDeleteById(gEngineHandle, gFilterIdFlow);
		gFilterIdFlow = 0;
	}

	//删除 callout 
	FwpmCalloutDeleteByKey(gEngineHandle, &CALLOUT_INBOUND_GUID);
	FwpmCalloutDeleteByKey(gEngineHandle, &CALLOUT_OUTBOUND_GUID);
	FwpmCalloutDeleteByKey(gEngineHandle, &CALLOUT_FLOW_GUID);

	//关闭 engine
	FwpmEngineClose(gEngineHandle);
	gEngineHandle = NULL;

	//注销 callout
	if (gCalloutIdInbound)
	{
		FwpsCalloutUnregisterById(gCalloutIdInbound);
		gCalloutIdInbound = 0;
	}

	if (gCalloutIdOutbound)
	{
		FwpsCalloutUnregisterById(gCalloutIdOutbound);
		gCalloutIdOutbound = 0;
	}

	if (gCalloutIdFlow)
	{
		FwpsCalloutUnregisterById(gCalloutIdFlow);
		gCalloutIdFlow = 0;
	}
}