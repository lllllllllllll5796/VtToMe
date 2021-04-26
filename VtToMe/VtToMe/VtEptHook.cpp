#include "VtEptHook.h"
#include "LDE64x64.h"
#include "VtBase.h"
#include "Tools.h"
#include "VtEpt.h"
#include "VtAsm.h"

// ����
extern  VtEpt *g_Ept;
extern	VtBase * g_Vmxs[128];

VtEptHookEntry m_EptHookRootPointer; // hook list ָ��
ULONG hooklist_len = 0; // hook list ����

VtEptHook::VtEptHook()
{
}

VtEptHook::~VtEptHook()
{
}

// �ṩ Ept Hook ��ʽ
// �滻����ҳ�ķ�ʽHook
void * VtEptHook::VtEptHookMemory(
	IN ULONG_PTR HookGuestLinerAddress,	 // Ҫhook��Ŀ�����Ե�ַ
	IN ULONG_PTR JmpLinerAddress,		 // Ҫ��ת�����Ե�ַ
	IN ULONG HookMode)					 // X86 or X64
{
	UNREFERENCED_PARAMETER(HookMode);

	// �ж� EPT �Ƿ����� �Լ� ��ǰ���Ƿ����� EPT
	ULONG cpu_number = KeGetCurrentProcessorNumber();
	if (!g_Ept && !g_Vmxs[cpu_number]->VtIsUseEpt)
	{
		//kprint(("EPT δ����!!!\r\n"));
		return NULL;
	}

	// ���� VtEptHookEntry �ṹ��
	VtEptHookEntry * hook_item = (VtEptHookEntry *)kmalloc(sizeof(VtEptHookEntry));
	if (!hook_item)
	{
		//kprint(("hook_item ����ʧ��!\r\n"));
		return NULL;
	}

	// ��� VtEptHookEntry �ṹ��Ļ����ֶ�
	hook_item->hook_guest_liner_address = HookGuestLinerAddress;
	hook_item->jmp_liner_address = JmpLinerAddress;
	hook_item->guest_cr3 = VtBase::VmCsRead(GUEST_CR3);
	hook_item->itemid = hooklist_len++;
	hook_item->ishook = TRUE;
	// �ҵ� HOOK ��
	if (m_EptHookRootPointer.hooklist.Flink == NULL)
	{
		InitializeListHead(&m_EptHookRootPointer.hooklist); // ��ʼ������
	}
	InsertTailList(&m_EptHookRootPointer.hooklist, &hook_item->hooklist); // ��������

	// �ر�д�������л� Cr3
	RemovWP();
	__writecr3(hook_item->guest_cr3);

	// ���� jmp shell code
	VtJmpShellCodeInformationEntry jmp_information = { 0 };
	if (!VtGetJmpShellCode(HookGuestLinerAddress, JmpLinerAddress, &jmp_information))
	{
		kprint(("VtGetJmpShellCode error!\r\n"));
		return NULL;
	}

	// ��� VtEptHookEntry �ṹ��������ַ��Ϣ�ֶ�
	hook_item->fake_phy_address = MmGetPhysicalAddress((PVOID)jmp_information.FakePageLinerAddress).QuadPart & 0xFFFFFFFFFFFFF000;
	hook_item->real_phy_address = MmGetPhysicalAddress((PVOID)HookGuestLinerAddress).QuadPart & 0xFFFFFFFFFFFFF000;
	hook_item->guest_fake_pte_pointer = reinterpret_cast<ULONG_PTR>(g_Ept->VtGetPteByPhyAddress(hook_item->fake_phy_address));
	hook_item->guest_fake_pte_pointer = reinterpret_cast<ULONG_PTR>(g_Ept->VtGetPteByPhyAddress(hook_item->real_phy_address));

	// ��ԭд�������л� Cr3
	__writecr3(hook_item->guest_cr3);  // �ָ� Cr3
	UnRemovWP();

	// ����hook��ҳ��Ϊִֻ��
	pEptPtEntry hookItem = (pEptPtEntry)hook_item->guest_fake_pte_pointer;
	hookItem->Bits.execute_access = 1;
	hookItem->Bits.read_access = 0;
	hookItem->Bits.write_access = 0;
	hookItem->Bits.physial_address = hook_item->fake_phy_address >> 12;

	// ˢ�� EPT
	// INVEPT ָ������ṩ�� EPTP.PML4T ��ַ��ˢ�� Guest PhySical Mapping �Լ� Combined Mapping ��ص� Cache ��Ϣ
	// Type Ϊ 2 ʱ(���л�����Ч), ˢ�� EPTP.PML4T ��ַ�µ����� Cache ��Ϣ
	__invept(2, &g_Ept->m_Eptp.all);

	return (void *)jmp_information.OriginalFunHeadCode; // ����ԭ��������
}

// �ṩ Ept Delete All Hook ��ʽ
void VtEptHook::VtEptDeleteAllHook()
{
	if (!hooklist_len) return void();
	//__debugbreak();
	for (PLIST_ENTRY pListEntry = m_EptHookRootPointer.hooklist.Flink;
		pListEntry != &m_EptHookRootPointer.hooklist;
		pListEntry = pListEntry->Flink)
	{
		pVtEptHookEntry pEntry = CONTAINING_RECORD(pListEntry, VtEptHookEntry, hooklist);
		pEptPtEntry pte = g_Ept->VtGetPteByPhyAddress(pEntry->real_phy_address);

		// ����pte
		pte->Bits.physial_address = pEntry->real_phy_address >> 12;
		pte->Bits.execute_access = 1;
		pte->Bits.write_access = 1;
		pte->Bits.read_access = 1;

		// �ͷ��ڴ�
		kFree(pEntry);
		hooklist_len--;

		if (!hooklist_len)
		{
			// ????????????????? ��� BUG ?????????????????
			break;
		}
	}

	return void();
}

// �ṩ����ָ��HookItem��ʽ
VtEptHookEntry * VtEptHook::VtGetEptHookItemByPhyAddress(IN ULONG_PTR phy_address)
{
	if (!phy_address || 
		(m_EptHookRootPointer.hooklist.Flink == NULL) || 
		IsListEmpty(&m_EptHookRootPointer.hooklist))
	{
		kprint(("�յ� hook list error!\r\n"));
		return NULL;
	}

	// ȥ������λ����
	phy_address &= 0xFFFFFFFFFFFFF000;

	// ���������ַ��ѭ������ hook list
	for (PLIST_ENTRY pListEntry = m_EptHookRootPointer.hooklist.Flink;
		pListEntry != &m_EptHookRootPointer.hooklist;
		pListEntry = pListEntry->Flink)
	{
		pVtEptHookEntry pEntry = CONTAINING_RECORD(pListEntry, VtEptHookEntry, hooklist);
		if ((phy_address == pEntry->fake_phy_address ||
			phy_address == pEntry->real_phy_address) &&
			phy_address)
		{
			return pEntry; // �ҵ�Ŀ�꣬����
		}
	}

	return NULL;
}

// �ṩ������תShellCode�ķ�ʽ
bool VtEptHook::VtGetJmpShellCode(
	IN ULONG_PTR HookGuestLinerAddress,				// Ҫhook��Ŀ�����Ե�ַ
	IN ULONG_PTR JmpLinerAddress,					// Ҫ��ת�����Ե�ַ
	OUT VtJmpShellCodeInformationEntry * jmp_info)	// jmp shell code ��Ϣ�ṹ��
{
	/*
		������������
		push �����ַ
		ret
		�ķ�ʽ��HOOK

		����������ǧ������jmp qword ptr [***]�ķ�ʽ��
		�������ȡ��ָ��֮��ĵ�ַ(��ָ��֮��ĵ�ַ�洢��������ַ)
		���²�ͣ����EptViolation
	*/
	PCHAR OriginalFunHeadCode = NULL;
	ULONG_PTR FakeLinerAddr = HookGuestLinerAddress;
	ULONG_PTR JmpLinerAddr = JmpLinerAddress;
	UCHAR JmpFakeAddr[] = "\x48\xB8\x00\x00\x00\x00\x00\x00\x00\x00\x50\xC3"; // return ��ȥ����
	UCHAR JmpOriginalFun[] = "\xFF\x25\x00\x00\x00\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"; // JMP ��������

	// �鿴�Ƿ��Ѿ��� hook
	pVtEptHookEntry hook_item =
		VtGetEptHookItemByPhyAddress(MmGetPhysicalAddress((PVOID)HookGuestLinerAddress).QuadPart);

	if (hook_item != NULL)
	{
		// �Ѿ��� hook ��
		if (hook_item->ishook == TRUE)
		{
			// hook ��δȡ��
			kprint(("��λ���ѱ� hook���һ���������!\r\n"));
			return false;
		}
	}

	// ������� hook �� new or cover
	/*
		������ת�� ShellCode
	*/
	RtlMoveMemory(JmpFakeAddr + 2, &JmpLinerAddr, 8);

	// ��������ȥ�Ĵ���
	ULONG_PTR WriteLen = GetWriteCodeLen((PVOID)FakeLinerAddr, 12);
	ULONG_PTR JmpOriginalAddr = FakeLinerAddr + WriteLen;
	RtlMoveMemory(JmpOriginalFun + 6, &JmpOriginalAddr, 8);
	
	// ����ԭ����ҳ��
	ULONG_PTR MyFakePage = (ULONG_PTR)kmalloc(PAGE_SIZE);
	RtlMoveMemory((PVOID)MyFakePage, (PVOID)(FakeLinerAddr & 0xFFFFFFFFFFFFF000), PAGE_SIZE);

	// ���� ����ԭ�������޸ĵĴ��� �� ����ԭ����
	OriginalFunHeadCode = (PCHAR)kmalloc(WriteLen + 14);
	RtlFillMemory(OriginalFunHeadCode, WriteLen + 14, 0x90);
	RtlMoveMemory(OriginalFunHeadCode, (PVOID)FakeLinerAddr, WriteLen);
	RtlMoveMemory((PCHAR)(OriginalFunHeadCode)+WriteLen, JmpOriginalFun, 14);

	// ��������ִ�еļ�ҳ��
	ULONG_PTR offset = FakeLinerAddr - (FakeLinerAddr & 0xFFFFFFFFFFFFF000); // ��ȡ��� PTE BASE ��ƫ��
	RtlFillMemory((PVOID)(MyFakePage + offset), WriteLen, 0x90);
	RtlMoveMemory((PVOID)(MyFakePage + offset), &JmpFakeAddr, 12);

	// ��д jmp shell code information
	jmp_info->FakePageLinerAddress = MyFakePage;
	jmp_info->OriginalFunHeadCode = (ULONG_PTR)OriginalFunHeadCode;

	return true;
}

// �����ṩ�� Ept Hook �򻯽ӿ�
bool VtEptHook::VtSimplifyEptHook(
	IN void * hook_liner_address,	 // Ҫhook��Ŀ�����Ե�ַ
	IN void * jmp_liner_address,	 // Ҫ��ת�����Ե�ַ
	OUT void ** ret_address)			 // X86 or X64(�ݲ�ʹ��)
{
	if (!MmIsAddressValid(ret_address) && !hook_liner_address && !jmp_liner_address)
	{
		kprint(("��������!\r\n"));
		return false;
	}
	
	LARGE_INTEGER timeOut;
	timeOut.QuadPart = -1 * 1000 * 1000; // 0.1���ӳټ���, �Է� VT δ����
	KeDelayExecutionThread(KernelMode, FALSE, &timeOut);

	// Ept Hook ����
	Asm_VmxCall(CallEptHook, hook_liner_address, jmp_liner_address, ret_address);

	if (!ret_address) {
		kprint(("VtSimplifyEptHook ����ʧ��!\r\n"));
		return false;
	}

	return true;
}

