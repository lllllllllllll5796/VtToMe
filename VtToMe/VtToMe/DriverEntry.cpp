#include "VtInstall.h"
#include "VtEptHook.h"
#include "Tools.h"
#include "VtSsdtHook.h"
#include "VtAsm.h"
#include <ntifs.h>
#include <intrin.h>

typedef NTSTATUS (NTAPI*pFakeZwClose)(
	_In_ HANDLE Handle
);
pFakeZwClose g_ZwClose;

EXTERN_C
NTSTATUS FASTCALL NTAPI FakeZwClose(
	_In_ HANDLE Handle
)
{
	KdPrint(("���� FakeZwClose!\r\n"));
	NTSTATUS status = STATUS_SUCCESS;
	KdBreakPoint();
	status = g_ZwClose(Handle);

	return status;
}

EXTERN_C
VOID DriverUnload(PDRIVER_OBJECT DriverObject)
{
	UNREFERENCED_PARAMETER(DriverObject);
	KdPrint(("[+] ����ж��ing...\n"));

	VtInstall::VtSimplifyStop();
}

EXTERN_C
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegisterPath)
{
	UNREFERENCED_PARAMETER(RegisterPath);
	NTSTATUS Status = STATUS_SUCCESS;
	KdPrint(("[+] [%#p]����������ing...\r\n", DriverEntry));
	DriverObject->DriverUnload = DriverUnload;

	VtInformaitonEntry vt_information = { 0 };
	vt_information.driver_object = DriverObject;	// ���ݲ���
	vt_information.u1.Bits.user_ept = TRUE;			// ���� EPT
	vt_information.u1.Bits.user_ssdt_hook = TRUE;	// ���� SSDT HOOK
	// ����VT
	VtInstall::VtSimplifyStart(vt_information); 
	
	LARGE_INTEGER timeOut;
	timeOut.QuadPart = -1 * 1000 * 1000; // 0.1���ӳټ���, �Է� VT δ����
	KeDelayExecutionThread(KernelMode, FALSE, &timeOut);

	

	

	//g_ZwClose = ZwClose;
	//VtSsdtHook::VtHookSsdtByIndex(SsdtIndex(&ZwClose), NtClose, 1);

	// ���� OpCode ��ȡ, Win10X64 NtOpenProcess ��ַ
	//char opcode[26] = {
	//	"\x48\x89\x5c\x24\x08\x48\x89\x6c\x24\x10\x48\x89\x74\x24\x18\x57\x48\x83\xec\x20\x8b\x79\x18\x33\xdb"
	//};
	//PVOID ntopenprocess_func_address = (PCHAR)MmFindByCode(opcode, 25) - 0x30;
	//if (ntopenprocess_func_address) {
	//	KdPrint(("NtOpenProcess Address: %#p", ntopenprocess_func_address));
	//}
	//
	//// ���� VT ����
	//bool result_bool = VtEptHook::VtSimplifyEptHook(ntopenprocess_func_address, FakeNtOpenProcess, (PVOID*)&NtOpenProcessRetAddr);
	//if (!result_bool) {
	//	KdPrint(("[+] NtOpenProcess Hook ʧ��!\r\n"));
	//}
	
	return Status;
}