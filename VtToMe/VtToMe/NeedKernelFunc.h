#pragma once

#ifndef KERNELFUNC
#define KERNELFUNC

// ��ͷ�ļ����ڶ���δ�������ں˺���

#include <ntifs.h>

EXTERN_C 
_IRQL_requires_(DISPATCH_LEVEL) // ������ DISPATCH_LEVEL �ȼ�����ú���
_IRQL_requires_same_			// ������ ����ĵȼ� �˳��ú���
VOID KeGenericCallDpc(//
	_In_ PKDEFERRED_ROUTINE Routine,
	_In_opt_ PVOID Context
);

EXTERN_C 
_IRQL_requires_(DISPATCH_LEVEL) // ������ DISPATCH_LEVEL �ȼ�����ú���
_IRQL_requires_same_			// ������ ����ĵȼ� �˳��ú���
LOGICAL KeSignalCallDpcSynchronize(
	_In_ PVOID SystemArgument2
);

EXTERN_C 
_IRQL_requires_(DISPATCH_LEVEL) // ������ DISPATCH_LEVEL �ȼ�����ú���
_IRQL_requires_same_			// ������ ����ĵȼ� �˳��ú���
VOID KeSignalCallDpcDone(
	_In_ PVOID SystemArgument1
);

EXTERN_C // ��ȡ EPROCESS �ṹ�еĽ�������
PCHAR PsGetProcessImageFileName(PEPROCESS Process);

EXTERN_C // ��ͣ����
NTSTATUS PsSuspendProcess(PEPROCESS Process);

#endif
