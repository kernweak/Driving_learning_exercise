#include <ntddk.h>
#define  OPER1 CTL_CODE(FILE_DEVICE_UNKNOWN,0x800,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define  OPER2 CTL_CODE(FILE_DEVICE_UNKNOWN,0x900,METHOD_BUFFERED,FILE_ANY_ACCESS)
NTSTATUS DefaultProc(DEVICE_OBJECT *DeviceObject, IRP *Irp);
NTSTATUS WriteProc(DEVICE_OBJECT *DeviceObject, IRP *Irp);
NTSTATUS ReadProc(DEVICE_OBJECT *DeviceObject, IRP *Irp);
NTSTATUS CreateProc(DEVICE_OBJECT *DeviceObject, IRP *Irp);
NTSTATUS ControlProc(DEVICE_OBJECT *DeviceObject, IRP *Irp);
NTSTATUS CloseProc(DEVICE_OBJECT *DeviceObject, IRP *Irp);
VOID DriverUnload(PDRIVER_OBJECT pDriver);
NTSTATUS DriverEntry(PDRIVER_OBJECT pDriver, PUNICODE_STRING pPath) {
	UNREFERENCED_PARAMETER(pPath);
	KdPrint(("Driver Entry\r\n"));
	DbgBreakPoint();
	PDEVICE_OBJECT pDevice = NULL;//设备类型的指针
	UNICODE_STRING Devicename;
	RtlInitUnicodeString(&Devicename, L"\\Device\\Hello");
	UNICODE_STRING strSymbolicName;
	RtlInitUnicodeString(&strSymbolicName, L"\\DosDevices\\Hello");
	//1.创建设备
	IoCreateDevice(pDriver, 0, &Devicename, FILE_DEVICE_UNKNOWN, 0, FALSE, &pDevice);
	//告诉三环数据交互方式
	pDevice->Flags |= DO_BUFFERED_IO;
	//|= DO_DIRECT_IO;  //直接读写方式，使用MDL重映射报证安全
	//绑定一个符号链接
	IoCreateSymbolicLink(&strSymbolicName, &Devicename);
	//设置IRP程序
	for (int i = 0;i < IRP_MJ_MAXIMUM_FUNCTION;i++) {

		pDriver->MajorFunction[i] = DefaultProc;
		pDriver->MajorFunction[IRP_MJ_CREATE] = CreateProc;
		pDriver->MajorFunction[IRP_MJ_WRITE] = WriteProc;
		pDriver->MajorFunction[IRP_MJ_READ] = ReadProc;
		pDriver->MajorFunction[IRP_MJ_DEVICE_CONTROL] = ControlProc;
		pDriver->MajorFunction[IRP_MJ_CLOSE] = CloseProc;
	}
	pDriver->DriverUnload = DriverUnload;
	return STATUS_SUCCESS;
}
NTSTATUS ControlProc(
	DEVICE_OBJECT *DeviceObject,
	IRP *pIrp
)
{


	UNREFERENCED_PARAMETER(DeviceObject);
	NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;
	PIO_STACK_LOCATION pIrpStack;
	ULONG uIoControlCode;
	PVOID pIoBuffer;
	ULONG uInLength;
	ULONG uOutLength;
	ULONG uRead;
	ULONG uWrite;

	//设置零时变量的值
	uRead = 0;
	TCHAR dwInBufer[15] = { 0 };
	uWrite = 0x12345678;
	//获取IRP数据
	pIrpStack = IoGetCurrentIrpStackLocation(pIrp);
	//获取控制码
	uIoControlCode = pIrpStack->Parameters.DeviceIoControl.IoControlCode;
	//获取缓冲区地址（输入输出缓冲区都是一个）
	pIoBuffer = pIrp->AssociatedIrp.SystemBuffer;
	//获取Ring3发送数据长度
	uInLength = pIrpStack->Parameters.DeviceIoControl.InputBufferLength;
	//获取Ring0发送数据的长度
	uOutLength = pIrpStack->Parameters.DeviceIoControl.OutputBufferLength;

	switch (uIoControlCode)
	{
	case OPER1:
		DbgPrint("OPER1....\n");
		pIrp->IoStatus.Information = 0;
		status = STATUS_SUCCESS;
		break;
	case OPER2:
		DbgPrint("IrpDeviceControlProc->OPER2 接受字节数：%d \n", uInLength);
		
		//Read From Buffer
		memcpy(dwInBufer, (TCHAR*)pIoBuffer, uInLength+1);
		DbgPrint("IrpDeviceControlProc->OPER2 ...%s \n", dwInBufer);
		
		//Write to Buffer
		memcpy(pIoBuffer, &uWrite, uOutLength);
		DbgPrint("IrpDeviceControlProc->OPER2 发送字节数：%d \n", uOutLength);
		DbgPrint("%s", pIoBuffer);

		pIrp->IoStatus.Information = uOutLength;
		status = STATUS_SUCCESS;
		break;
	default:
		break;
	}

	//设置IRP完成状态
	pIrp->IoStatus.Status = STATUS_SUCCESS;//向3环返回状态,不设置默认是失败
										   //设置IRP操作了多少字节
	pIrp->IoStatus.Information = 20;
	//处理IRP
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

NTSTATUS CreateProc(
	DEVICE_OBJECT *DeviceObject,
	IRP *pIrp
)
{

	UNREFERENCED_PARAMETER(DeviceObject);
	DbgPrint("DispatchCreate!\n");

	pIrp->IoStatus.Status = STATUS_SUCCESS;//向3环返回状态
										   //设置IRP操作了多少字节
	pIrp->IoStatus.Information = 20;
	//处理IRP
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

NTSTATUS CloseProc(
	DEVICE_OBJECT *DeviceObject,
	IRP *pIrp
)
{

	UNREFERENCED_PARAMETER(DeviceObject);
	DbgPrint("DispatchClose!\n");

	pIrp->IoStatus.Status = STATUS_SUCCESS;//向3环返回状态
										   //设置IRP操作了多少字节
	pIrp->IoStatus.Information = 20;
	//处理IRP
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

NTSTATUS DefaultProc(
	DEVICE_OBJECT *DeviceObject,
	IRP *pIrp
)
{

	UNREFERENCED_PARAMETER(DeviceObject);
	//设置IRP完成状态
	pIrp->IoStatus.Status = STATUS_SUCCESS;//向3环返回状态,不设置默认是失败
										   //设置IRP操作了多少字节
	pIrp->IoStatus.Information = 20;
	//处理IRP
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

NTSTATUS WriteProc(
	DEVICE_OBJECT *DeviceObject,
	IRP *pIrp
)
{

	UNREFERENCED_PARAMETER(DeviceObject);
	//设置IRP完成状态
	pIrp->IoStatus.Status = STATUS_SUCCESS;//向3环返回状态,不设置默认是失败
										   //设置IRP操作了多少字节
	pIrp->IoStatus.Information = 20;
	//处理IRP
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}
NTSTATUS ReadProc(
	DEVICE_OBJECT *DeviceObject,
	IRP *pIrp
)
{

	UNREFERENCED_PARAMETER(DeviceObject);
	//设置IRP完成状态
	pIrp->IoStatus.Status = STATUS_SUCCESS;//向3环返回状态,不设置默认是失败
										   //设置IRP操作了多少字节
	pIrp->IoStatus.Information = 20;
	//处理IRP
	IoCompleteRequest(pIrp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}

VOID DriverUnload(PDRIVER_OBJECT pDriver) {
	KdPrint(("Leave Driver!"));
	UNICODE_STRING strSymbolicName;
	RtlInitUnicodeString(&strSymbolicName, L"\\DosDevices\\Hello");
	IoDeleteSymbolicLink(&strSymbolicName);
	IoDeleteDevice(pDriver->DeviceObject);
}