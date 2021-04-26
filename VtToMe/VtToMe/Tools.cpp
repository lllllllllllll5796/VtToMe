#include "Tools.h"
#include <intrin.h>

typedef struct _KernelMdouleInfo
{
	ULONG_PTR Base; // ��ַ
	ULONG_PTR Size; // ��С
}KernelMdouleInfo, *PKernelMdouleInfo;

KernelMdouleInfo VtKernelInfo;
KIRQL irQl;

/* ���ڻ�ȡ�ں�ҳ������ */
ULONG64 g_NT_BASE;
ULONG64 g_PTE_BASE = 0xFFFFF90000000000;
ULONG64 g_PDE_BASE = 0xFFFFF97C80000000;
ULONG64 g_PPE_BASE = 0xFFFFF97CBE400000;
ULONG64 g_PXE_BASE = 0xFFFFF97CBE5F2000;

// �޸�Cr0�Ĵ���, ȥ��д�������ڴ汣�����ƣ�
KIRQL RemovWP()
{
	//DbgPrint("RemovWP\n");
	// (PASSIVE_LEVEL)���� IRQL �ȼ�ΪDISPATCH_LEVEL�������ؾɵ� IRQL
	// ��Ҫһ���ߵ�IRQL�����޸�
	irQl = KeRaiseIrqlToDpcLevel();
	ULONG_PTR cr0 = __readcr0(); // ������������ȡCr0�Ĵ�����ֵ, �൱��: mov eax,  cr0;

	// ����16λ��WPλ����0������д����
	cr0 &= ~0x10000; // ~ ��λȡ��
	_disable(); // ����жϱ��, �൱�� cli ָ��޸� IF��־λ
	__writecr0(cr0); // ��cr0������������д��Cr0�Ĵ����У��൱��: mov cr0, eax
	//DbgPrint("�˳�RemovWP\n");
	return irQl;
}

// ��ԭCr0�Ĵ���
KIRQL UnRemovWP()
{
	//DbgPrint("UndoWP\n");
	ULONG_PTR cr0 = __readcr0();
	cr0 |= 0x10000; // WP��ԭΪ1
	_disable(); // ����жϱ��, �൱�� cli ָ���� IF��־λ
	__writecr0(cr0); // ��cr0������������д��Cr0�Ĵ����У��൱��: mov cr0, eax

	// �ָ�IRQL�ȼ�
	KeLowerIrql(irQl);
	//DbgPrint("�˳�UndoWP\n");
	return irQl;
}

// ��ȡ����ϵͳ�汾
ULONG GetWindowsVersion()
{
	RTL_OSVERSIONINFOW lpVersionInformation = { sizeof(RTL_OSVERSIONINFOW) };
	if (NT_SUCCESS(RtlGetVersion(&lpVersionInformation)))
	{
		ULONG dwMajorVersion = lpVersionInformation.dwMajorVersion;
		ULONG dwMinorVersion = lpVersionInformation.dwMinorVersion;
		if (dwMajorVersion == 5 && dwMinorVersion == 1)
		{
			return WINXP;
		}
		else if (dwMajorVersion == 6 && dwMinorVersion == 1)
		{
			return WIN7;
		}
		else if (dwMajorVersion == 6 && dwMinorVersion == 2)
		{
			return WIN8;
		}
		else if (dwMajorVersion == 10 && dwMinorVersion == 0)
		{
			return WIN10;
		}
	}
	return FALSE;
}

// ��ȡ�����ַ��Ӧ��Pte
// �� ring3 ���ڴ�ӳ�䵽 ring0 ������һ���ں� LinerAddress
VOID * GetKernelModeLinerAddress(ULONG_PTR cr3, ULONG_PTR user_mode_address)
{
	PHYSICAL_ADDRESS cr3_phy = { 0 };
	cr3_phy.QuadPart = cr3;
	ULONG_PTR current_cr3 = 0;
	PVOID cr3_liner_address = NULL;

	PHYSICAL_ADDRESS user_phy = { 0 };
	PVOID kernel_mode_liner_address = NULL;

	// �ж�cr3�Ƿ���ȷ	
	cr3_liner_address = MmGetVirtualForPhysical(cr3_phy);
	if (!MmIsAddressValid(cr3_liner_address)) {
		kprint(("cr3 ��������!\r\n"));
		return NULL;
	}
	// �ж��Ƿ�Ϊ rin3 �ĵ�ַ �Լ� ��ַ�Ƿ�ɶ�ȡ
	else if (user_mode_address >= 0xFFFFF80000000000) {
		// ���Ϊ�ں˵�ַ, ����Ҫӳ��
		kprint(("user_mode_address Ϊ�ں˵�ַ!\r\n"));
		return (void *)user_mode_address;
	}
	// �����ַ���ɶ�
	else if (!MmIsAddressValid((void *)user_mode_address)) {
		kprint(("user_mode_address ��������!\r\n"));
		return NULL;
	}
	
	current_cr3 = __readcr3();
	// �ر�д�������л�Cr3
	RemovWP();
	__writecr3(cr3_phy.QuadPart);

	// ӳ�� user mode �ڴ�	
	user_phy = MmGetPhysicalAddress((void*)user_mode_address);
	//PVOID kernel_mode_liner_address = MmGetVirtualForPhysical(user_phy); //(ֱ�ӷֽ�PTE����ʽ��ȡ��Ӧ�������ַ)
	kernel_mode_liner_address = MmMapIoSpace(user_phy, 10, MmNonCached); // ӳ��rin3�ڴ浽rin0

	// �ָ�
	__writecr3(current_cr3);
	UnRemovWP();

	if (kernel_mode_liner_address) {
		return kernel_mode_liner_address;
	}
	else
		return NULL;
}

// �� ring3 ���ڴ�ӳ�䵽 ring0 ������һ���ں� LinerAddress
VOID FreeKernelModeLinerAddress(VOID * p, size_t size)
{
	if ((ULONG_PTR)p < 0xFFFFF80000000000) {
		if (p && size) {
			MmUnmapIoSpace(p, size);
		}
	}
}

// ��ȡ�ں�ģ���ַ����С 
// (ͨ����������, MmFindByCode ������Ҫ�˺�����ִ��)
NTSTATUS GetKernelMoudleBaseAndSize(
	IN PDRIVER_OBJECT DriverObject,
	OUT PULONG_PTR szBase,
	OUT PULONG_PTR szSize)
{
	NTSTATUS dwStatus = STATUS_UNSUCCESSFUL;
	UNICODE_STRING dwKernelMoudleName;
	RtlInitUnicodeString(&dwKernelMoudleName, L"ntoskrnl.exe");

	// ��ȡ��������, ����ģ��
	PLDR_DATA_TABLE_ENTRY dwEentry = (PLDR_DATA_TABLE_ENTRY)(DriverObject->DriverSection);
	PLIST_ENTRY  dwFirstentry = NULL;
	PLIST_ENTRY  dwpCurrententry = NULL;
	PLDR_DATA_TABLE_ENTRY pCurrentModule = NULL;

	if (dwEentry)
	{
		dwFirstentry = dwEentry->InLoadOrderLinks.Flink;
		dwpCurrententry = dwFirstentry->Flink;

		while (dwFirstentry != dwpCurrententry)
		{
			// ��ȡLDR_DATA_TABLE_ENTRY�ṹ
			pCurrentModule = CONTAINING_RECORD(dwpCurrententry, LDR_DATA_TABLE_ENTRY, InLoadOrderLinks);

			if (pCurrentModule->BaseDllName.Buffer != 0)
			{
				if (RtlCompareUnicodeString(&dwKernelMoudleName, &(pCurrentModule->BaseDllName), FALSE) == 0)
				{
					*szBase = (__int64)pCurrentModule->DllBase;
					*szSize = (__int64)pCurrentModule->SizeOfImage;

					dwStatus = STATUS_SUCCESS;
					return dwStatus;
				}
			}
			// ��һ��
			dwpCurrententry = dwpCurrententry->Flink;
		}
	}
	return dwStatus;
}

// �ڴ��������ҵ�ַ
// szCode: ������
// szSize: �������С (ע���ַ�����\00��β�����ױ���)
PVOID MmFindByCode(char * szCode, size_t szSize)
{
	if (szCode && szSize)
	{
		PCHAR dwKernelBase = (PCHAR)VtKernelInfo.Base;

		for (unsigned __int64 i = 0; i < VtKernelInfo.Size; i++)
		{
			// �ж��ں˵�ַ�Ƿ�ɶ�
			if (!MmIsAddressValid(&dwKernelBase[i]))
			{
				continue; // ���ɶ�, ��ʼ��һ��
			}

			for (unsigned __int64 j = 0x0; j < szSize; j++)
			{
				// �ж��ں˵�ַ�Ƿ�ɶ�
				if (!MmIsAddressValid(&dwKernelBase[i + j]))
				{
					continue; // ���ɶ�, ��ʼ��һ��
				}

				// ֧��ģ������
				if (szCode[j] == '*')
				{
					// ����ѭ��
					continue;
				}

				if (dwKernelBase[i + j] != szCode[j])
				{
					// ��һ���ڴ�Ƚϲ���ȣ�������ǰѭ��
					break;
				}

				if (j + 1 == szSize)
				{
					// ���ص�ַ
					return (PVOID)(&dwKernelBase[i]);
				}
			}
		}
	}

	return NULL;
}

// ��ȡ PXE
PULONG64 GetPxeAddress(PVOID addr)
{
	// 1�� PXE ��Ӧ 512 GB
	return (PULONG64)(((((ULONG64)addr & 0xFFFFFFFFFFFF) >> 39) << 3) + g_PXE_BASE);
}

// ��ȡ PDPTE
PULONG64 GetPpeAddress(PVOID addr)
{
	// 1�� PDPTE ��Ӧ 1 GB
	return (PULONG64)(((((ULONG64)addr & 0xFFFFFFFFFFFF) >> 30) << 3) + g_PPE_BASE);
}

// ��ȡ PDE
PULONG64 GetPdeAddress(PVOID addr)
{
	// 1�� PDE ��Ӧ 2 MB
	return (PULONG64)(((((ULONG64)addr & 0xFFFFFFFFFFFF) >> 21) << 3) + g_PDE_BASE);
}

// ��ȡ PTE
PULONG64 GetPteAddress(PVOID addr)
{
	// 1�� PTE ��Ӧ 4KB
	return (PULONG64)(((((ULONG64)addr & 0xFFFFFFFFFFFF) >> 12) << 3) + g_PTE_BASE);
}

// ��ʼ��������
NTSTATUS VtInitTools(PDRIVER_OBJECT DriverObject)
{
	NTSTATUS dwStatus = STATUS_SUCCESS;

	// ��ȡ�ں�ģ����Ϣ
	dwStatus = GetKernelMoudleBaseAndSize(DriverObject, &VtKernelInfo.Base, &VtKernelInfo.Size);
	if (!NT_SUCCESS(dwStatus))
	{
		KdPrint(("GetKernelMoudleBaseAndSize Error: [%X]", dwStatus));
		return dwStatus;
	}
	
	// ��� Win10 ���Ŀ¼ҳ��
	g_NT_BASE = VtKernelInfo.Base;
	/*g_PTE_BASE = *(PULONG64)(g_NT_BASE + 0x3D68 + 0x2);
	g_PDE_BASE = (ULONG64)GetPteAddress((PVOID)g_PTE_BASE);
	g_PPE_BASE = (ULONG64)GetPteAddress((PVOID)g_PDE_BASE);
	g_PXE_BASE = (ULONG64)GetPteAddress((PVOID)g_PPE_BASE);*/

	return dwStatus;
}