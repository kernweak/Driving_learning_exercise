#include <ntifs.h>
#include <ntimage.h>

#pragma pack(1)
typedef struct _ServiceDesriptorEntry
{
	ULONG *ServiceTableBase;        // ������ַ
	ULONG *ServiceCounterTableBase; // �������ַ
	ULONG NumberOfServices;         // ������ĸ���
	UCHAR *ParamTableBase;          // �������ַ
}SSDTEntry, *PSSDTEntry;

#pragma pack()
// ����SSDT,�����ȫ�ֱ���
NTSYSAPI SSDTEntry KeServiceDescriptorTable;

PSSDTEntry pNewSSDT;

PCHAR g_pHookpointer;
PCHAR g_pJmpPointer;
VOID DriverUnload(PDRIVER_OBJECT pDriver) {
	UNREFERENCED_PARAMETER(pDriver);
}

HANDLE KernelCreateFile(
	IN PUNICODE_STRING pstrFile, // �ļ�·����������
	IN BOOLEAN         bIsDir)   // �Ƿ�Ϊ�ļ���
{
	HANDLE hFile = NULL;
	NTSTATUS Status = STATUS_SUCCESS;
	IO_STATUS_BLOCK StatusBlock = { 0 };
	ULONG ulShareAccess=
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
	ULONG uLCreateOpt= FILE_SYNCHRONOUS_IO_NONALERT;
	//1. ��ʼ��OBJECT_ATTRIBUTES������
	OBJECT_ATTRIBUTES objAttrib = { 0 };
	ULONG ulAttributes =
		OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE;//�����ִ�Сд|�þ��ֻ�����ں�ģʽ����
	InitializeObjectAttributes(
		&objAttrib,    // ���س�ʼ����ϵĽṹ��
		pstrFile,      // �ļ���������
		ulAttributes,  // ��������
		NULL, NULL);   // һ��ΪNULL
	//2. �����ļ�����
	uLCreateOpt |= bIsDir ?
		FILE_DIRECTORY_FILE : FILE_NON_DIRECTORY_FILE;
	Status = ZwCreateFile(
		&hFile,                // �����ļ����
		GENERIC_ALL,           // �ļ���������
		&objAttrib,            // OBJECT_ATTRIBUTES
		&StatusBlock,          // ���ܺ����Ĳ������
		0,                     // ��ʼ�ļ���С
		FILE_ATTRIBUTE_NORMAL, // �½��ļ�������
		ulShareAccess,         // �ļ�����ʽ
		FILE_OPEN_IF,          // �ļ�������򿪲������򴴽�
		uLCreateOpt,           // �򿪲����ĸ��ӱ�־λ
		NULL,                  // ��չ������
		0);                   // ��չ����������
	if (!NT_SUCCESS(Status))
		return (HANDLE)-1;
	return hFile;

}

ULONG64 KernelGetFileSize(IN HANDLE hfile) 
{
	// ��ѯ�ļ�״̬
	IO_STATUS_BLOCK           StatusBlock = { 0 };
	FILE_STANDARD_INFORMATION fsi = { 0 };
	NTSTATUS Status = STATUS_UNSUCCESSFUL;
	Status = ZwQueryInformationFile(
		hfile,        // �ļ����
		&StatusBlock, // ���ܺ����Ĳ������
		&fsi,         // �������һ��������������������Ϣ
		sizeof(FILE_STANDARD_INFORMATION),
		FileStandardInformation);
	if (!NT_SUCCESS(Status))
		return 0;
	return fsi.EndOfFile.QuadPart;
}
ULONG64 KernelReadFile(
	IN  HANDLE         hfile,
	IN  PLARGE_INTEGER Offset,
	IN  ULONG          ulLength,
	OUT PVOID          pBuffer)
{
	// 1. ��ȡ�ļ�
	IO_STATUS_BLOCK StatusBlock = { 0 };
	NTSTATUS        Status = STATUS_UNSUCCESSFUL;
	Status = ZwReadFile(
		hfile,        // �ļ����
		NULL,         // �ź�״̬(һ��ΪNULL)
		NULL, NULL,   // ����
		&StatusBlock, // ���ܺ����Ĳ������
		pBuffer,      // �����ȡ���ݵĻ���
		ulLength,     // ��Ҫ��ȡ�ĳ���
		Offset,       // ��ȡ����ʼƫ��
		NULL);        // һ��ΪNULL
	if (!NT_SUCCESS(Status))  return 0;
	// 2. ����ʵ�ʶ�ȡ�ĳ���
	return StatusBlock.Information;
}

typedef struct _LDR_DATA_TABLE_ENTRY {
	LIST_ENTRY InLoadOrderLinks;    //˫������
	LIST_ENTRY InMemoryOrderLinks;
	LIST_ENTRY InInitializationOrderLinks;
	PVOID DllBase;
	PVOID EntryPoint;
	ULONG SizeOfImage;
	UNICODE_STRING FullDllName;
	UNICODE_STRING BaseDllName;
	ULONG Flags;
	USHORT LoadCount;
	USHORT TlsIndex;


} LDR_DATA_TABLE_ENTRY, *PLDR_DATA_TABLE_ENTRY;

PVOID GetModuleBase(PDRIVER_OBJECT pDriver, PUNICODE_STRING pModuleName)
{
	PLDR_DATA_TABLE_ENTRY pLdr =
		(PLDR_DATA_TABLE_ENTRY)pDriver->DriverSection;
	LIST_ENTRY *pTemp = &pLdr->InLoadOrderLinks;
	do
	{
		PLDR_DATA_TABLE_ENTRY pDriverInfo =
			(PLDR_DATA_TABLE_ENTRY)pTemp;
		KdPrint(("%wZ\n", &pDriverInfo->FullDllName));
		if (
			RtlCompareUnicodeString(pModuleName, &pDriverInfo->BaseDllName, FALSE)
			== 0)
		{
			return pDriverInfo->DllBase;
		}
		pTemp = pTemp->Blink;
	} while (pTemp != &pLdr->InLoadOrderLinks);
	return 0;
}


//��ȡ�ں��ļ����ڴ棬���ҽ���չ��
void GetReLoadBuf(PUNICODE_STRING KerPath, PCHAR* pReloadBuf)
{
	LARGE_INTEGER Offset = { 0 };
	HANDLE hFile = KernelCreateFile(KerPath, FALSE);
	ULONG64 uSize = KernelGetFileSize(hFile);
	PCHAR pKernelBuf = ExAllocatePool(NonPagedPool, (SIZE_T)uSize);
	RtlZeroMemory(pKernelBuf, (SIZE_T)uSize);
	KernelReadFile(hFile,&Offset, (ULONG)uSize, pKernelBuf);
	// չ���ں��ļ�
	PIMAGE_DOS_HEADER pDos = (PIMAGE_DOS_HEADER)pKernelBuf;
	PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)(pDos->e_lfanew + pKernelBuf);
	PIMAGE_SECTION_HEADER pSection = IMAGE_FIRST_SECTION(pNt);
	*pReloadBuf = ExAllocatePool(NonPagedPool, pNt->OptionalHeader.SizeOfImage);//�ڴ�
	RtlZeroMemory(*pReloadBuf, pNt->OptionalHeader.SizeOfImage);
	//2.1 �ȿ���PEͷ��
	RtlCopyMemory(*pReloadBuf, pKernelBuf, pNt->OptionalHeader.SizeOfHeaders);


	//2.2 �ٿ���PE��������
	for (size_t i = 0; i < pNt->FileHeader.NumberOfSections; i++)
	{
		RtlCopyMemory(
			*pReloadBuf + pSection[i].VirtualAddress,//�ڴ��ƫ��
			pKernelBuf + pSection[i].PointerToRawData,//�ļ��е�ƫ��
			pSection[i].SizeOfRawData
		);
	}
	ExFreePool(pKernelBuf);

}

void FixReloc(PCHAR OldKernelBase, PCHAR NewKernelBase)
{
	typedef struct _TYPEOFFSET
	{

		USHORT Offset : 12;
		USHORT type : 4;
	}TYPEOFFSET, *PTYPEOFFSET;
	//1 �ҵ��ض�λ��
	PIMAGE_DOS_HEADER pDos = (PIMAGE_DOS_HEADER)NewKernelBase;

	PIMAGE_NT_HEADERS pNt = (PIMAGE_NT_HEADERS)(pDos->e_lfanew + NewKernelBase);

	PIMAGE_DATA_DIRECTORY pDir = (pNt->OptionalHeader.DataDirectory + 5);
	PIMAGE_BASE_RELOCATION pReloc = (PIMAGE_BASE_RELOCATION)(NewKernelBase+pDir->VirtualAddress);
	while (pReloc->SizeOfBlock != 0)
	{
		//2 Ѱ���ض�λ��λ��
		ULONG uCount = (pReloc->SizeOfBlock - 8) / 2;
		PCHAR pSartAddress = (pReloc->VirtualAddress + NewKernelBase);
		PTYPEOFFSET pOffset = (PTYPEOFFSET)(pReloc + 1);
		for (ULONG i = 0; i < uCount; i++)
		{
			if (pOffset->type == 3)
			{
				//3 ��ʼ�ض�λ
				//NewBase-DefaultBase = NewReloc-DefaultReloc
				ULONG * pRelocAdd = (ULONG *)(pSartAddress + pOffset->Offset);
				*pRelocAdd += ((ULONG)OldKernelBase - pNt->OptionalHeader.ImageBase);
			}
			pOffset++;
		}

		pReloc = (PIMAGE_BASE_RELOCATION)((PCHAR)pReloc + pReloc->SizeOfBlock);
	}
}

void FixSSDT(PCHAR OldKernelBase, PCHAR NewKernelBase)
{
	//���ں��е�ĳλ�� - NewKernelBase = ���ں��еĴ�λ�� - OldKernelBase
	//���ں��е�ĳλ��  = NewKernelBase-OldKernelBase+���ں��еĴ�λ��
	ULONG uOffset = (ULONG)NewKernelBase - (ULONG)OldKernelBase;

	pNewSSDT =
		(PSSDTEntry)((PCHAR)&KeServiceDescriptorTable + uOffset);
	//���SSDT��������
	pNewSSDT->NumberOfServices =
		KeServiceDescriptorTable.NumberOfServices;
	//���SSDT������ַ��
	pNewSSDT->ServiceTableBase =
		(PULONG)((PCHAR)KeServiceDescriptorTable.ServiceTableBase + uOffset);
	for (ULONG i = 0; i < pNewSSDT->NumberOfServices; i++)
	{
		pNewSSDT->ServiceTableBase[i] = pNewSSDT->ServiceTableBase[i] + uOffset;
	}
	//���SSDT����������һ��Ԫ��ռ1���ֽ�
	pNewSSDT->ParamTableBase =
		((UCHAR*)KeServiceDescriptorTable.ParamTableBase + uOffset);

	memcpy(pNewSSDT->ParamTableBase, KeServiceDescriptorTable.ParamTableBase,
		KeServiceDescriptorTable.NumberOfServices);

	//���SSDT���ô�����,����һ��Ԫ��ռ4���ֽ�
	//pNewSSDT->ServiceCounterTableBase =
	//	(PULONG)((PCHAR)KeServiceDescriptorTable.ServiceCounterTableBase + uOffset);

	//memcpy(pNewSSDT->ServiceCounterTableBase, 
	//	KeServiceDescriptorTable.ServiceCounterTableBase,
	//	KeServiceDescriptorTable.NumberOfServices*4
	//	);

}


void * SearchMemory(char * buf, int BufLenth, char * Mem, int MaxLenth)
{
	int MemIndex = 0;
	int BufIndex = 0;
	for (MemIndex = 0; MemIndex < MaxLenth; MemIndex++)
	{
		BufIndex = 0;
		if (Mem[MemIndex] == buf[BufIndex] || buf[BufIndex] == '?')
		{
			int MemIndexTemp = MemIndex;
			do
			{
				MemIndexTemp++;
				BufIndex++;
			} while ((Mem[MemIndexTemp] == buf[BufIndex] || buf[BufIndex] == '?') && BufIndex < BufLenth);
			if (BufIndex == BufLenth)
			{
				return Mem + MemIndex;
			}
		}
	}
	return 0;
}

PVOID GetKiFastCallEntryAddr()
{
	PVOID pAddr = 0;
	_asm
	{
		push ecx;
		push eax;
		mov ecx, 0x176;
		rdmsr;
		mov pAddr, eax;
		pop eax;
		pop ecx;
	}
	return pAddr;
}
void OffProtect()
{
	__asm { //�ر��ڴ汣��
		push eax;
		mov eax, cr0;
		and eax, ~0x10000;
		mov cr0, eax;
		pop eax;
	}

}
void OnProtect()
{
	__asm { //�����ڴ汣��
		push eax;
		mov eax, cr0;
		OR eax, 0x10000;
		mov cr0, eax;
		pop eax;
	}
}
UCHAR CodeBuf[] = { 0x2b, 0xe1, 0xc1, 0xe9, 0x02 };
UCHAR NewCodeBuf[5] = { 0xE9 };

ULONG  FilterSSDT(ULONG uCallNum, PULONG FunBaseAddress, ULONG FunAdress)
{
	//˵������������õ�SSDT��������shdowSSDT
	if (FunBaseAddress == KeServiceDescriptorTable.ServiceTableBase)
	{
		if (uCallNum == 190)
		{
			return pNewSSDT->ServiceTableBase[190];
		}
	}
	return FunAdress;
}

_declspec(naked) void MyFilterFunction()
{
	//5 ���Լ���Hook�������ж����Լ����ں˻���ԭ�����ں�
	//eax ���ú�
	//edi SSDT�������ַ
	//edx �����Ǵ�SSDT���л�õĺ����ĵ�ַ
	_asm
	{
		pushad;
		pushfd;
		push edx;
		push edi;
		push eax;
		call FilterSSDT;
		mov dword ptr ds : [esp + 0x18], eax;
		popfd;
		popad;
		sub     esp, ecx;
		shr     ecx, 2;
		jmp g_pJmpPointer;
	}
}

void OnHookKiFastCall()
{
	//1 �ȵõ�KifastcallEntry�ĵ�ַ
	PVOID KiFastCallAdd = GetKiFastCallEntryAddr();
	//2 ����2b e1 c1 e9 02
	g_pHookpointer = SearchMemory(CodeBuf, 5, KiFastCallAdd, 0x200);
	//3 �ҵ����λ��֮��ֱ��hook
	//3.1�ر�ҳ����
	OffProtect();
	//3.2�滻5���ֽ�
	*(ULONG*)(NewCodeBuf + 1) =
		((ULONG)MyFilterFunction - (ULONG)g_pHookpointer - 5);
	memcpy(g_pHookpointer, NewCodeBuf, 5);

	//3.3����ҳ����
	OnProtect();
	g_pJmpPointer = g_pHookpointer + 5;
}

NTSTATUS DriverEntry(PDRIVER_OBJECT pDriver, PUNICODE_STRING pPath) {
	UNREFERENCED_PARAMETER(pPath);
	DbgBreakPoint();
	PCHAR pNtModuleBase = NULL;
	UNICODE_STRING pNtModuleName;
	//1.�ҵ��ں��ļ���������չ��
	//��ʼ���ַ���
	PCHAR pReloadBuf = NULL;
	UNICODE_STRING KerPath;
	RtlInitUnicodeString(&KerPath, L"\\??\\C:\\windows\\system32\\ntkrnlpa.exe");

	GetReLoadBuf(&KerPath, &pReloadBuf);
	//2 �޸��ض�λntoskrnl.exe
	RtlInitUnicodeString(&pNtModuleName, L"ntoskrnl.exe");
	pNtModuleBase = (PCHAR)GetModuleBase(pDriver, &pNtModuleName);
	FixReloc(pNtModuleBase, pReloadBuf);
	//3 �޸��Լ���SSDT��
	FixSSDT(pNtModuleBase, pReloadBuf);

	//4 Hook KiFastCallEntry������ϵͳ����
	OnHookKiFastCall();
	pDriver->DriverUnload = DriverUnload;
	return STATUS_SUCCESS;
}