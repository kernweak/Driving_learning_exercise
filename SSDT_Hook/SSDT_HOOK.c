#include <ntddk.h>
#include<ntstatus.h>
//1.�ҵ�ϵͳ�����ĺ�����ַ��
//����һ��ȫ�ֱ����������֮ǰ��NtOpenProcess��ַ
ULONG uOldNtOpenProcess;
//���˵�ַ����Ҫһ������NtOpenProcessָ�룬���ڵ���ԭ����NtOpenProcess

//�����޸��ͻָ�ҳ���Եĺ���
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
	PULONG  ServiceTableBase;                               		// ��������ַ���ַ  
	PULONG  ServiceCounterTableBase;
	ULONG   NumberOfService;                                		// �������ĸ���  
	PULONG   ParamTableBase;                                		// �������������ַ   
} KSYSTEM_SERVICE_TABLE, *PKSYSTEM_SERVICE_TABLE;

typedef struct _KSERVICE_TABLE_DESCRIPTOR
{
	KSYSTEM_SERVICE_TABLE   ntoskrnl;                       // ntoskrnl.exe �ķ�����  
	KSYSTEM_SERVICE_TABLE   win32k;                         // win32k.sys �ķ�����(GDI32.dll/User32.dll ���ں�֧��)  
	KSYSTEM_SERVICE_TABLE   notUsed1;
	KSYSTEM_SERVICE_TABLE   notUsed2;
}KSERVICE_TABLE_DESCRIPTOR, *PKSERVICE_TABLE_DESCRIPTOR;


//������ ntoskrnl�������� SSDT
extern PKSERVICE_TABLE_DESCRIPTOR KeServiceDescriptorTable;//����ǵ����ģ�Ҫ���ں��ļ��ң��������ֲ���Ϲ��

//׼�������滻�ĺ���
NTSTATUS NTAPI MyNtOpenProcess(__out PHANDLE  ProcessHandle,
	__in ACCESS_MASK  DesiredAccess,
	__in POBJECT_ATTRIBUTES  ObjectAttributes,
	__in_opt PCLIENT_ID  ClientId
)
{
	NTSTATUS Status;
	Status = STATUS_SUCCESS;
	//�������Լ���ҵ�񡣡������ֹ��ˣ��޸ķ��ؽṹ��
	KdPrint(("MyNtOpenProcess %x %x %x %x \n", ProcessHandle, DesiredAccess, ObjectAttributes, ClientId));
	//������������Ǵ�ԭ���ĺ�������Ϊ�������ҲҪʵ��ԭ���Ĺ��ܣ���Ȼ�������ˣ��������Լ����Լ�ҵ����ʵ����
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
	__asm { //�ر��ڴ汣��
		push eax;
		mov eax, cr0;
		and eax, ~0x10000;
		mov cr0, eax;
		pop eax;
	}
}

void PageProtectOn() {
	////�������ͷ�MDL
	//if (MDSystemCall)
	//{
	//	MmUnmapLockedPages(MappedSCT, MDSystemCall);
	//	IoFreeMdl(MDSystemCall);
	//}
	__asm { //�ָ��ڴ汣��
		push eax;
		mov eax, cr0;
		or eax, 0x10000;
		mov cr0, eax;
		pop eax;
	}
}
//3.�޸ĺ�����ַ,׼�������������޸ĺ�����ַ
void HookNtOpenProcess() {
	NTSTATUS Status;
	Status = STATUS_SUCCESS;
	PageProtectOff();
	uOldNtOpenProcess = KeServiceDescriptorTable->ntoskrnl.ServiceTableBase[0xBE];
	KeServiceDescriptorTable->ntoskrnl.ServiceTableBase[0xBE] = (ULONG)MyNtOpenProcess;
	PageProtectOn();
}
//4.�ָ�
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
	KdPrint(("->%x \n", KeServiceDescriptorTable->ntoskrnl.ServiceTableBase[0xBE]));//�õ�������ַ��




	HookNtOpenProcess();




	pDriver->DriverUnload = DriverUnload;
	return STATUS_SUCCESS;
}