#pragma once
#ifndef VTSSDTHOOK
#define VTSSDTHOOK

#include "VtHeader.h"

// ʹ�� VT ���� SSDT Hook
// Win7 ��ʹ�� MSR HOOK��Win10 ʹ�� EFER HOOK

#define MAX_SYSCALL_INDEX 0x1000 // ��������˱��СΪ 0x1000

class VtSsdtHook : public VtHeader
{
public:
	VtSsdtHook();
	~VtSsdtHook();

public:
	// ��ʼ������
	static bool VtInitSsdtHook();
	// Hook ָ���±� SSDT ����
	static bool VtHookSsdtByIndex(ULONG32 ssdt_index, PVOID hook_address, CHAR param_number);

public:
	// Hook Msr Lstar �Ĵ���
	static bool VtHookMsrLstar();
	// Un Hook Msr Lstar �Ĵ���
	static bool VtUnHookMsrLstar();

public:
	// Efer Hook
	static bool VtEferHook();
	// #UD �쳣����
	static bool UdExceptionVtExitHandler(ULONG_PTR * Registers);
	// ģ�� SysCall ����
	static bool VtSysCallEmulate(ULONG_PTR * Registers);
	// ģ�� SysRet ����
	static bool VtSysRetEmulate(ULONG_PTR * Registers);

public:
	// ����SsdtHook
	static bool VtStartHookSsdt();
	// ֹͣSsdtHook
	static bool VtStopHookSsdt();
};

#endif

