#pragma once
#ifndef TOOLS
#define TOOLS

#include "VtHeader.h"

/***************************************************�ں˹��ܺ���**********************************************************/
// �޸�Cr0�Ĵ���, ȥ��д�������ڴ汣�����ƣ�
KIRQL RemovWP();
// ��ԭCr0�Ĵ���
KIRQL UnRemovWP();
// ��ȡ����ϵͳ�汾
ULONG GetWindowsVersion();

// �� ring3 ���ڴ�ӳ�䵽 ring0 ������һ���ں� LinerAddress
VOID * GetKernelModeLinerAddress(ULONG_PTR cr3, ULONG_PTR user_mode_address);
// �� ring3 ���ڴ�ӳ�䵽 ring0 ������һ���ں� LinerAddress
VOID FreeKernelModeLinerAddress(VOID * p, size_t size = 10);

// ��ȡ PXE
PULONG64 GetPxeAddress(PVOID addr);
// ��ȡ PDPTE
PULONG64 GetPpeAddress(PVOID addr);
// ��ȡ PDE
PULONG64 GetPdeAddress(PVOID addr);
// ��ȡ PTE
PULONG64 GetPteAddress(PVOID addr);

/***************************************************�ں��ڴ���Ϣ����**********************************************************/
// ��ȡ�ں�ģ���ַ����С (ͨ����������)
NTSTATUS GetKernelMoudleBaseAndSize(
	IN PDRIVER_OBJECT DriverObject,
	OUT PULONG_PTR szBase,
	OUT PULONG_PTR szSize);
// �ں��ڴ��������ҵ�ַ
// szCode: ������
// szSize: �������С
PVOID MmFindByCode(char * szCode, size_t szSize);
/**************************************************************************************************************************/

// ��ʼ��������
NTSTATUS VtInitTools(PDRIVER_OBJECT DriverObject);

#endif

