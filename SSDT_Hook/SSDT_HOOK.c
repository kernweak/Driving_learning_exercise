#include <ntddk.h>
#include<ntstatus.h>
//1.找到系统服务表的函数地址表
//定义一个全局变量用来存放之前的NtOpenProcess地址
ULONG uOldNtOpenProcess;
//有了地址还需要一个函数NtOpenProcess指针，用于调用原来的NtOpenProcess

//定义修复和恢复页属性的函数
PMDL MDSystemCall;
PVOID *MappedSCT;




typedef NTSTATUS(*NTOPENPROCESS)(
	__out PHANDLE  ProcessHandle,
	__in ACCESS_MASK  DesiredAccess,
	__in POBJECT_ATTRIBUTES  ObjectAttributes,
	__in_opt PCLIENT_ID  ClientId
	);

typedef struct _KSYSTEM_SERVICE_TABLE
{
	PULONG  ServiceTableBase;                               		// 服务函数地址表基址  
	PULONG  ServiceCounterTableBase;
	ULONG   NumberOfService;                                		// 服务函数的个数  
	PULONG   ParamTableBase;                                		// 服务函数参数表基址   
} KSYSTEM_SERVICE_TABLE, *PKSYSTEM_SERVICE_TABLE;

typedef struct _KSERVICE_TABLE_DESCRIPTOR
{
	KSYSTEM_SERVICE_TABLE   ntoskrnl;                       // ntoskrnl.exe 的服务函数  
	KSYSTEM_SERVICE_TABLE   win32k;                         // win32k.sys 的服务函数(GDI32.dll/User32.dll 的内核支持)  
	KSYSTEM_SERVICE_TABLE   notUsed1;
	KSYSTEM_SERVICE_TABLE   notUsed2;
}KSERVICE_TABLE_DESCRIPTOR, *PKSERVICE_TABLE_DESCRIPTOR;


//导出由 ntoskrnl所导出的 SSDT
extern PKSERVICE_TABLE_DESCRIPTOR KeServiceDescriptorTable;//这个是导出的，要到内核文件找，所以名字不能瞎起

//准备用于替换的函数
NTSTATUS NTAPI MyNtOpenProcess(__out PHANDLE  ProcessHandle,
	__in ACCESS_MASK  DesiredAccess,
	__in POBJECT_ATTRIBUTES  ObjectAttributes,
	__in_opt PCLIENT_ID  ClientId
)
{
	NTSTATUS Status;
	Status = STATUS_SUCCESS;
	//这里填自己的业务。。。各种过滤，修改返回结构等
	KdPrint(("MyNtOpenProcess %x %x %x %x \n", ProcessHandle, DesiredAccess, ObjectAttributes, ClientId));
	//后面这里填的是打开原来的函数，因为这个函数也要实现原来的功能，不然就乱套了，除非你自己在自己业务里实现了
	return ((NTOPENPROCESS)uOldNtOpenProcess)(ProcessHandle, DesiredAccess, ObjectAttributes, ClientId);
}

void PageProtectOff() {

	//MDSystemCall = MmCreateMdl(NULL, KeServiceDescriptorTable->ntoskrnl.ServiceTableBase, KeServiceDescriptorTable->ntoskrnl.NumberOfService * 4);
	//if (!MDSystemCall)
	//	//return STATUS_UNSUCCESSFUL;
	//	return;
	//MmBuildMdlForNonPagedPool(MDSystemCall);
	//MDSystemCall->MdlFlags = MDSystemCall->MdlFlags | MDL_MAPPED_TO_SYSTEM_VA;
	//MappedSCT = MmMapLockedPages(MDSystemCall, KernelMode);
	__asm { //关闭内存保护
		push eax;
		mov eax, cr0;
		and eax, ~0x10000;
		mov cr0, eax;
		pop eax;
	}
}

void PageProtectOn() {
	////解锁、释放MDL
	//if (MDSystemCall)
	//{
	//	MmUnmapLockedPages(MappedSCT, MDSystemCall);
	//	IoFreeMdl(MDSystemCall);
	//}
	__asm { //恢复内存保护
		push eax;
		mov eax, cr0;
		or eax, 0x10000;
		mov cr0, eax;
		pop eax;
	}
}
//3.修改函数地址,准备个函数用来修改函数地址
void HookNtOpenProcess() {
	NTSTATUS Status;
	Status = STATUS_SUCCESS;
	PageProtectOff();
	uOldNtOpenProcess = KeServiceDescriptorTable->ntoskrnl.ServiceTableBase[0xBE];
	KeServiceDescriptorTable->ntoskrnl.ServiceTableBase[0xBE] = (ULONG)MyNtOpenProcess;
	PageProtectOn();
}
//4.恢复
void UnHookNtOpenProcess() {
	PageProtectOff();
	KeServiceDescriptorTable->ntoskrnl.ServiceTableBase[0xBE] = (ULONG)uOldNtOpenProcess;
	PageProtectOn();
}

VOID DriverUnload(PDRIVER_OBJECT pDriver) {
	UNREFERENCED_PARAMETER(pDriver);
	UnHookNtOpenProcess();

	KdPrint(("My Dirver is unloading..."));

}

NTSTATUS DriverEntry(PDRIVER_OBJECT pDriver, PUNICODE_STRING pPath) {
	UNREFERENCED_PARAMETER(pPath);
	KdPrint(("->%x \n", KeServiceDescriptorTable->ntoskrnl.ServiceTableBase[0xBE]));//得到函数地址表




	HookNtOpenProcess();




	pDriver->DriverUnload = DriverUnload;
	return STATUS_SUCCESS;
}