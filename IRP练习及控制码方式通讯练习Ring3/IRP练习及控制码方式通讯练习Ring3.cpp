// IRP练习及控制码方式通讯练习.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include <Windows.h>
#define  OPER1 CTL_CODE(FILE_DEVICE_UNKNOWN,0x800,METHOD_BUFFERED,FILE_ANY_ACCESS)
#define  OPER2 CTL_CODE(FILE_DEVICE_UNKNOWN,0x900,METHOD_BUFFERED,FILE_ANY_ACCESS)

int main()
{

	HANDLE hFile = CreateFile(
		L"\\??\\Hello",
		FILE_ALL_ACCESS,
		NULL,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL
	);
	TCHAR dwInBufer[15] = { /*_TEXT("abcde")*/0 };
	//DWORD dwInBufer = 0x11112222;
	printf("请输出字符串\n");
	_tscanf_s(L"%s", dwInBufer, 14);




	TCHAR szOutBuffer[20] = { 0 };
	DWORD x = 0;
	DeviceIoControl(hFile, OPER2, &dwInBufer, _tcslen(dwInBufer), szOutBuffer, 11, &x, 0);
	printf("%d\n", x);
	_tprintf(L"%s", szOutBuffer);
	system("pause");
	return 0;
}



