#include "VtEpt.h"
#include "VtBase.h"
#include "VtEptHook.h"
#include "LDE64x64.h"
#include "Tools.h"
#include <intrin.h>

// PDPTE 1GB PDE 2MB��PTE 4KB
// PDPTT 512GB��PDT 1GB��PT 2MB
// 8 GB �ڴ�ֻ��Ҫ 1��PML4T��1��PDPT��8��PDT��512 * 8��PDE(һ��PDE��Ӧ4KB��PT�ڴ�)
#define  TOTAL_MEM  32 // 32GB�ڴ����

// ��������ڴ���׵�ַ, ����˼���ǲ���һ�����ڴ����ʽ�������ͬ�� VMM(Guest)
PCHAR EptMem = NULL;

VtEpt::VtEpt()
{
	m_Eptp = { 0 };
	LDE_init(); // ��ʼ���������
}

VtEpt::~VtEpt()
{
	
}

// ����	EPT
BOOLEAN VtEpt::VtStartEpt()
{
	if (EptMem)
	{
		// ȫ�� Ept �Ѿ�����
		kprint(("ȫ�� Ept �Ѿ�����!\r\n"));
		return TRUE;
	}

	// ����Ҫע�����е� VMM ʹ�õ�ʱһ�� EPT �ڴ�
	if (!VtInitEpt())
	{
		kprint(("ȫ�� Ept ��ʼ��ʧ��!\r\n"));
		return FALSE;
	}

	kprint(("[+]ȫ�� Ept ���гɹ�!\r\n"));

	return TRUE;
}

// �ر�   EPT
VOID VtEpt::VtCloseEpt()
{
	if (EptMem && MmIsAddressValid(EptMem))
	{
		// ж�����е�Ept Hook
		VtEptHook::VtEptDeleteAllHook(); //Win10 �� BUG ��ʱ�ر�
		
		kprint(("[+]���е� EPT HOOK ��ж��!\r\n"));

		// ж�� ept �ڴ�
		ExFreePoolWithTag(EptMem, 'eptm');
		EptMem = NULL;

		kprint(("[+]ȫ�� EPT ��ж��!\r\n"));
	}
}

// ��ʼ�� EPT
BOOLEAN VtEpt::VtInitEpt()
{
	ULONG_PTR Pageindex = 0; // ���ڼ�¼�ܷ�������ҳ������
	pEptPml4Entry Pml4t = NULL;
	pEptPdptEntry Pdpt = NULL;

	/*
		�����ڴ�(�ж�ϵͳ)...
	*/
	//KdBreakPoint();
	// ÿ��PML4E��PDPTE��PDE��PTE����ռ8���ֽ�
	// ÿ��PML4T��PDPT��PDT��PT����һҳ�ڴ�ռ4k
	// �����ܵ��ڴ棬����2����PML4T��PDPT��Ҫ����ҳ�ڴ棬TOTAL_MEM ��PDT��TOTAL_MEM * 512��PT
	// ע�������ڴ治Ҫʹ�� MmAllocateContiguousMemory
	EptMem = (PCHAR)ExAllocatePoolWithTag(NonPagedPool, (2 + TOTAL_MEM + TOTAL_MEM * 512) * PAGE_SIZE, 'eptm');
	
	if (NULL == EptMem)
	{
		kprint(("EptMem Ϊ NULL!\r\n"));
		return FALSE;
	}

	/*
		�����ڴ�...
	*/

	// �����ҳ��PML4T��PDPT����������һ��ÿ���СΪ4KB�����飬��һ��Ϊ(EptMem + 0 * PAGE_SIZE)
	// ���һ��Ϊ(EptMem + (1 + TOTAL_MEM + TOTAL_MEM * 512) * PAGE_SIZE)
	Pml4t = (pEptPml4Entry)(EptMem + (TOTAL_MEM + TOTAL_MEM * 512) * PAGE_SIZE);
	Pdpt = (pEptPdptEntry)(EptMem + (1 + TOTAL_MEM + TOTAL_MEM * 512) * PAGE_SIZE);

	/*
		�ܲ����������ģ�
		EptMem = {[PDT+512��PT],[PDT+512��PT],[PDT+512��PT]...��16��[PDT+512��PT]��PML4T��PDPT}
	*/

	// ���� Eptp ��Ϣ
	VtSetEptPointer(Pml4t);

	// ���� PML4E ��Ϣ
	Pml4t[0].all = MmGetPhysicalAddress(Pdpt).QuadPart & 0xFFFFFFFFFFFFFFF8; // ȥ������
	Pml4t[0].Bits.read_access = TRUE;
	Pml4t[0].Bits.write_access = TRUE;
	Pml4t[0].Bits.execute_access = TRUE;
	// ѭ������ PDPT/PDT/PT
	for (ULONG_PTR PDPT_Index = 0; PDPT_Index < TOTAL_MEM; PDPT_Index++)
	{
		// ����һҳ�� PDT
		pEptPdEntry Pdt = (pEptPdEntry)(EptMem + PAGE_SIZE * Pageindex++);
		// ���� PDPT ��Ϣ
		Pdpt[PDPT_Index].all = MmGetPhysicalAddress(Pdt).QuadPart & 0xFFFFFFFFFFFFFFF8;
		Pdpt[PDPT_Index].Bits.read_access = TRUE;
		Pdpt[PDPT_Index].Bits.write_access = TRUE;
		Pdpt[PDPT_Index].Bits.execute_access = TRUE;

		for (ULONG_PTR PDT_Index = 0; PDT_Index < 512; PDT_Index++)
		{
			// ����һҳ��PT
			pEptPtEntry Pt = (pEptPtEntry)(EptMem + PAGE_SIZE * Pageindex++);
			// ���� PDT ��Ϣ
			Pdt[PDT_Index].all = MmGetPhysicalAddress(Pt).QuadPart & 0xFFFFFFFFFFFFFFF8;
			Pdt[PDT_Index].Bits.read_access = TRUE;
			Pdt[PDT_Index].Bits.write_access = TRUE;
			Pdt[PDT_Index].Bits.execute_access = TRUE;

			for (ULONG_PTR PT_Index = 0; PT_Index < 512; PT_Index++)
			{
				// ���� PT ��GPA��Ϣ (�ο� �����������⻯������(��6.1.5��[419ҳ]))
				Pt[PT_Index].all = (PDPT_Index * (1 << 30) + PDT_Index * (1 << 21) + PT_Index * (1 << 12)); // ���� GPA
				Pt[PT_Index].Bits.read_access = TRUE;
				Pt[PT_Index].Bits.write_access = TRUE;
				Pt[PT_Index].Bits.execute_access = TRUE;
				Pt[PT_Index].Bits.memory_type = m_Eptp.Bits.memory_type;
			}
		}
	}

	kprint(("[+]ȫ�� Ept ��ʼ���ɹ�!\r\n"));

	return TRUE;
}

// ��ȡ EPT �����Ϣ
// @Pml4Address: PML4 �����Ե�ַ
VOID VtEpt::VtSetEptPointer(PVOID Pml4Address)
{
	// IA32�ֲ�24.6.11

	Ia32VmxEptVpidCapMsr ia32Eptinfo = { 0 };
	ia32Eptinfo.all = __readmsr(MSR_IA32_VMX_EPT_VPID_CAP);

	if (ia32Eptinfo.Bits.support_page_walk_length4)
	{
		m_Eptp.Bits.page_walk_length = 3; // ����Ϊ 4 ��ҳ��
		kprint(("[+]֧��4����ҳ\r\n"));
	}

	if (ia32Eptinfo.Bits.support_uncacheble_memory_type)
	{
		m_Eptp.Bits.memory_type = 0; // UC(�޻������͵��ڴ�)
		kprint(("[+]֧��Ept ʹ��UC�ڴ�\r\n"));
	}

	if (ia32Eptinfo.Bits.support_write_back_memory_type)
	{
		m_Eptp.Bits.memory_type = 6; // WB(�ɻ�д���͵��ڴ�, ֧������������)
		kprint(("[+]֧��Ept ʹ��WB�ڴ�\r\n"));
	}

	if (ia32Eptinfo.Bits.support_accessed_and_dirty_flag) // Ept dirty ��־λ�Ƿ���Ч
	{
		m_Eptp.Bits.enable_accessed_and_dirty_flags = TRUE;
	}
	else
	{
		m_Eptp.Bits.enable_accessed_and_dirty_flags = FALSE;
	}

	m_Eptp.Bits.pml4_address = MmGetPhysicalAddress(Pml4Address).QuadPart / PAGE_SIZE; // ��յ� 3 �ֽ�(����)

	kprint(("[+]EptPointer �������!\r\n"));
	return VOID();
}

// ͨ�������ַ��ȡ PTE ָ��
// @PhyAddress �����ַ
// @return Ept Pteָ��
pEptPtEntry VtEpt::VtGetPteByPhyAddress(ULONG_PTR PhyAddress)
{
	// ����9 9 9 9 12 ��ҳ��ȡGPA��Ӧ�ĸ������ EPT �±� (�ο� �����������⻯������(��419ҳ))
	ULONG_PTR PDPT_Index = (PhyAddress >> (9 + 9 + 12)) & 0x1FF;
	ULONG_PTR PDT_Index = (PhyAddress >> (9 + 12)) & 0x1FF;
	ULONG_PTR PT_Index = (PhyAddress >> 12) & 0x1FF;

	/*
		�ܲ����������ģ�
		EptMem = {[PDT+512��PT],[PDT+512��PT],[PDT+512��PT]...��16��[PDT+512��PT]��PML4T��PDPT}
	*/

	// ����EptMem��һ��ÿ��Ԫ����һҳ��С�����飬offset���������±�
	// ���ÿһ�ȷݣ�ÿһ�ȷ���һ�� PDT + 512 ��PT���ų�����ҳ
	ULONG_PTR offset = 513;
	// �õ�Ŀ��ȷݣ���һҳ��PDT����Ҫ
	offset = offset * PDPT_Index + 1;
	// �õ���Ӧ��PT
	offset = offset + PDT_Index;

	ULONG_PTR* Pte = (ULONG_PTR*)(EptMem + offset * PAGE_SIZE) + PT_Index; // ��ȡ��Ӧ��PTE��������

	return reinterpret_cast<pEptPtEntry>(Pte);
}

// ���� Ept VM-exit �����Ļص�
VOID VtEpt::EptViolationVtExitHandler(ULONG_PTR * Registers)
{
	UNREFERENCED_PARAMETER(Registers);
	//KdBreakPoint();
	ULONG_PTR guestRip, guestRsp, guestPhyaddress;
	guestRip = guestRsp = guestPhyaddress = 0;

	guestRip = VtBase::VmCsRead(GUEST_RIP);
	guestRsp = VtBase::VmCsRead(GUEST_RSP);
	guestPhyaddress = VtBase::VmCsRead(GUEST_PHYSICAL_ADDRESS);

	// ͨ������ EptViolation �ĵ�ַ�������ǲ��Ǹ�����HOOK�й�
	pVtEptHookEntry hookItem = VtEptHook::VtGetEptHookItemByPhyAddress(guestPhyaddress);

	if (hookItem)
	{
		// ����� Hook �����У� ����
		// ��ȡ EPT PDT
		pEptPtEntry pte = VtGetPteByPhyAddress(guestPhyaddress);

		if (pte->Bits.execute_access)
		{
			// �����ҳ���޷���д�������쳣
			pte->Bits.execute_access = 0;
			pte->Bits.write_access = 1;
			pte->Bits.read_access = 1;
			pte->Bits.physial_address = hookItem->real_phy_address >> 12;
		}
		else
		{
			// �����ҳ���޷�ִ�д������쳣
			pte->Bits.execute_access = 1;
			pte->Bits.write_access = 0;
			pte->Bits.read_access = 0;
			pte->Bits.physial_address = hookItem->fake_phy_address >> 12;
		}
	}
	else
	{
		kprint(("δ֪ EPT ERROR!\r\n"));
		KdBreakPoint();
	}


	VtBase::VmCsWrite(GUEST_RIP, guestRip);
	VtBase::VmCsWrite(GUEST_RSP, guestRsp);

	// ˢ�� EPT
	// INVEPT ָ������ṩ�� EPTP.PML4T ��ַ��ˢ�� Guest PhySical Mapping �Լ� Combined Mapping ��ص� Cache ��Ϣ
	// Type Ϊ 2 ʱ(���л���), ˢ�� EPTP.PML4T ��ַ�µ����� Cache ��Ϣ
	__invept(2, &m_Eptp.all);

	return VOID();
}
