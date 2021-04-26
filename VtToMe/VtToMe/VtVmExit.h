#pragma once
#ifndef VTVMEXIT
#define VTVMEXIT

#include "VtHeader.h"

// ������Ҫ���ڴ��� VM-exit

// ����ͳһ���� VM-EXIT
EXTERN_C FASTCALL
VOID VtVmExitRoutine(ULONG_PTR * Registers);

// ���ڴ��� CPUID VM-EXIT
EXTERN_C
VOID CpuidVmExitHandler(ULONG_PTR * Registers);

// ���ڴ��� CrX VM-EXIT
EXTERN_C
VOID CrAccessVtExitHandler(ULONG_PTR * Registers);

// ���ڴ��� VMCALL VM-EXIT
EXTERN_C
VOID VmcallVmExitHandler(ULONG_PTR * Registers);

// �����ȡ MSR VM-EXIT
EXTERN_C 
VOID MsrReadVtExitHandler(ULONG_PTR * Registers);

// ����д�� MSR VM-EXIT
EXTERN_C
VOID MsrWriteVtExitHandler(ULONG_PTR * Registers);

// ���ڴ��� Nmi �ж�
EXTERN_C
VOID NmiExceptionVtExitHandler(ULONG_PTR * Registers);

// ���ڴ��� �ⲿ�ж�
EXTERN_C
VOID ExternalInterruptVtExitHandler(ULONG_PTR * Registers);

// ����� GDT/IDT ���ʵ��µ� VM-exit
EXTERN_C
VOID GdtrOrIdtrAccessVtExitHandler(ULONG_PTR * Registers);

// ����� LDT/TR ���ʵ��µ� VM-exit
EXTERN_C
VOID LdtrOrTrAccessVtExitHandler(ULONG_PTR * Registers);

// ���ڴ���Ĭ�� VM-EXIT
EXTERN_C
VOID DefaultVmExitHandler(ULONG_PTR * Registers);


#endif

