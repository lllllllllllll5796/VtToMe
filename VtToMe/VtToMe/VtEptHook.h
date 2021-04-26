#pragma once
#ifndef VTEPTHOOK
#define VTEPTHOOK

// �����ṩ���� EPT �� HOOK ����

#include "VtHeader.h"

// ���� Ept hook Entry
typedef struct _VtEptHookEntry
{
	ULONG itemid;						// Id
	ULONG_PTR hook_guest_liner_address; // Hook �� Guest �����Ե�ַ
	ULONG_PTR jmp_liner_address;		// Ҫ��������Ե�ַ
	ULONG_PTR guest_cr3;				// VM �� Cr3
	ULONG_PTR guest_fake_pte_pointer;	// Guest �����Ե�ַ�����ڵ� pte ���� pointer
	ULONG_PTR guest_real_pte_pointer;	// Guest �����Ե�ַ��ԭ���� pte ���� pointer
	ULONG_PTR fake_phy_address;			// �ٵ�����ҳ�������ַ
	ULONG_PTR real_phy_address;			// ԭ��������ҳ�������ַ
	LIST_ENTRY hooklist;				// EPT HOOK ����
	BOOLEAN ishook;						// ��hook�Ƿ���������
}VtEptHookEntry, *pVtEptHookEntry;

// ����洢 shellcode ��Ϣ�Ľṹ��
typedef struct _VtJmpShellCodeInformationEntry
{
	ULONG_PTR FakePageLinerAddress; // ��ҳ��� liner address
	ULONG_PTR OriginalFunHeadCode;	// ԭ�������̵� liner address
}VtJmpShellCodeInformationEntry, pVtJmpShellCodeInformationEntry;

class VtEptHook : public VtHeader
{
public:
	VtEptHook();
	~VtEptHook();

private:

public:
	// �ṩ Ept Hook ��ʽ
	static void * VtEptHookMemory(IN ULONG_PTR HookGuestLinerAddress, IN ULONG_PTR JmpLinerAddress, IN ULONG HookMode);
	// �ṩ Ept Delete All Hook ��ʽ
	static void VtEptDeleteAllHook();
	// �ṩ����ָ��HookItem��ʽ
	static VtEptHookEntry * VtGetEptHookItemByPhyAddress(IN ULONG_PTR phy_address);
	// �ṩ������תShellCode�ķ�ʽ
	static bool VtGetJmpShellCode(IN ULONG_PTR HookGuestLinerAddress, IN ULONG_PTR JmpLinerAddress, OUT VtJmpShellCodeInformationEntry * jmp_info);

public:
	// �����ṩ��Ept Hook�򻯽ӿ�
	static bool VtSimplifyEptHook(
		IN void * HookGuestLinerAddress = NULL,
		IN void * JmpLinerAddress = NULL,
		OUT void ** HookMode = NULL);
};

#endif

