#include "VtBase.h"
#include "VtEpt.h"

extern  VtEpt *g_Ept;

VtBase::VtBase()
{
	GuestRip = GuestRsp = 0;
	m_CpuNumber = 0;
	m_VmxOn = FALSE; // ��ǰ CPU �˵����⻯�Ƿ��
	m_VmOnRegionAddress = 0;     // VMON ����
	m_VmCsRegionAddress = 0;     // VMCS ����
	m_VmBitMapRegionAddress = 0; // VM BITMAP ����
	m_VmOnRegionPhyAddress = 0;    // ��Ӧ�������ַ
	m_VmCsRegionPhyAddress = 0;
	m_VmMsrBitMapRegionPhyAddress = 0;
	m_VmStackRootRegionPointer = 0;// VMM ����Ҫ�Ķ�ջ�ڴ�
	m_HostState = {0};  // HOST  ����
	m_GuestState = {0}; // GUEST ����
	VtIsUseEpt = FALSE; // �Ƿ�ʹ�� EPT
}

VtBase::~VtBase()
{}

// VMCS ����Ķ���
// @info: Ҫд����ֶ�
// @Value: Ҫд���ֵ
// @return ���� ULONG_PTR �� VMCS ��Ϣ, ���ɹ�Ϊ FALSE
BOOLEAN VtBase::VmCsWrite(ULONG_PTR info, ULONG_PTR Value)
{
	ULONG_PTR uinfo = info;

	if (__vmx_vmwrite(uinfo, Value) != 0)
	{
		__debugbreak();
		kprint(("VmcsField: [0x%016x] ����vmwriteʧ��!\n", info));
		return FALSE;
	}

	return TRUE;
}

// VMCS ����Ķ�ȡ
// @info: Ҫ��ȡ���ֶ�
// @return ���� ULONG_PTR �� VMCS ��Ϣ, ���ɹ�Ϊ -1
ULONG_PTR VtBase::VmCsRead(ULONG_PTR info)
{
	ULONG_PTR value = 0xFFFFFFFFFFFFFFFF;
	ULONG_PTR uinfo = info;

	if (__vmx_vmread(uinfo, &value) != 0)
	{
		__debugbreak();
		kprint(("Vmx [0x%016x] ����vmreadʧ��!\n", info));
		return value;
	}

	return value;
}

// ִ�� VMON ָ��
// @return ִ�гɹ����� TRUE
BOOLEAN VtBase::ExecuteVmxOn()
{
	ULONG_PTR m_VmxBasic = __readmsr(IA32_VMX_BASIC_MSR_CODE); // ��ȡ VMID
	// 1. ���汾�� VMID
	*(PULONG32)m_VmOnRegionAddress = (ULONG32)m_VmxBasic;
	*(PULONG32)m_VmCsRegionAddress = (ULONG32)m_VmxBasic;

	// 2. ����CR4
	Cr4 cr4 = { 0 };
	cr4.all = __readcr4();
	cr4.Bits.vmxe = TRUE; // CR4.VMXE ��Ϊ 1, ���� VMX ָ��
	__writecr4(cr4.all);

	// 3. ��ÿ�� CPU ���� VMXON ָ������
	Ia32FeatureControlMsr msr = { 0 };
	msr.all = __readmsr(IA32_FEATURE_CONTROL_CODE);
	if (!msr.Bits.lock)
	{
		msr.Bits.lock = TRUE;		  // ���� VMON
		msr.Bits.enable_vmxon = TRUE; // ���� VMON
		__writemsr(IA32_FEATURE_CONTROL_CODE, msr.all);
	}

	// 4. ִ�� VMON ָ��
	// VMXON ��ָ�����:
	// 1). VMXON ָ���Ƿ񳬹������ַ���, �����Ƿ� 4K ����
	// 2). VMXON �����ͷ DWORD ֵ�Ƿ���� VMCS ID
	// 3). ���Ὣ�������ַ��Ϊָ�룬ָ�� VMXON ����
	// 4). ���տ�ͨ�� eflags.cf �Ƿ�Ϊ 0 ���ж�ִ�гɹ�
	__vmx_on(&m_VmOnRegionPhyAddress);

	FlagRegister eflags = { 0 };
	*(ULONG_PTR*)(&eflags) = __readeflags();
	if (eflags.Bits.cf != 0)
	{
		kprint(("Cpu:[%d] vmxon ����ʧ��!\n", m_CpuNumber));
		return FALSE;
	}

	kprint(("[+]Cpu:[%d] vmxon �����ɹ�!\n", m_CpuNumber));
	m_VmxOn = TRUE; // ���� VT ���⻯��־

	// 5. ִ�� VMCLEAR ָ������ʼ�� VMCS ���ƿ���ض���Ϣ
	// ���� VMCS ����������ַ
	VtDbgErrorPrint(__vmx_vmclear(&m_VmCsRegionPhyAddress), "vmclear");
	// 6. ִ�� VMPTRLD ָ��� VMCS ����
	VtDbgErrorPrint(__vmx_vmptrld(&m_VmCsRegionPhyAddress), "vmptrld");

	kprint(("[+]Debug:[%d] VMCS װ�سɹ�\n", m_CpuNumber));
	return TRUE;
}

// ����Ƿ������� VT
// @return �������÷��� TRUE������ FALSE
BOOLEAN VtBase::VtCheckIsSupported()
{
	// ��⵱ǰ CPUD �Ƿ�֧�� VT ����
	// EAX = 1; CPUID.1:ECX.VMX[bit 5] �Ƿ�Ϊ 1
	unsigned __int32 Regs[4] = { 0 }; // EAX��EBX��ECX��EDX
	__cpuidex(reinterpret_cast<int *>(Regs), 1, 1);
	pCpudFeatureInfoByEcx pRcx = reinterpret_cast<pCpudFeatureInfoByEcx>(&Regs[2]);
	
	if (pRcx->Bits.vmx == FALSE)
	{
		return FALSE; // ��֧�����⻯
	}

	// ��� VMXON ָ���ܷ�ִ��
	// VMXON ָ���ܷ�ִ��Ҳ���� IA32_FEATURE_CONTROL_MSR �Ĵ����Ŀ���
	// IA32_FEATURE_CONTROL_MSR[bit 0] Ϊ 0, �� VMXON ���ܵ���
	// IA32_FEATURE_CONTROL_MSR[bit 2] Ϊ 0, �� VMXON ������ SMX ����ϵͳ�����
	ULONG_PTR uMsr = __readmsr(IA32_FEATURE_CONTROL_CODE);
	pIa32FeatureControlMsr pMsr = reinterpret_cast<pIa32FeatureControlMsr>(&uMsr);

	if ((pMsr->Bits.lock == FALSE) && (pMsr->Bits.enable_vmxon == FALSE))
	{
		return FALSE;
	}

	return TRUE;
}

// ������⻯�����Ƿ��
// @return ���⻯���ش򿪷��� TRUE������ FALSE
BOOLEAN VtBase::VtCheckIsEnable()
{
	// ���������ʱ���Ὣ CR4.VMXE[bit 13] ��Ϊ 1
	ULONG_PTR uCr4 = __readcr4();
	pCr4 Cr4Pointer = reinterpret_cast<pCr4>(&uCr4);

	if (Cr4Pointer->Bits.vmxe == TRUE)
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

// ���� VMON\VMCS �ڴ��ַ
// @return ����ɹ����� TRUE������ FALSE
BOOLEAN VtBase::VtVmmMemAllocate()
{
	// VMM �ڴ������
	m_VmOnRegionAddress = (ULONG_PTR *)kmalloc(PAGE_SIZE);
	m_VmCsRegionAddress = (ULONG_PTR *)kmalloc(PAGE_SIZE);
	m_VmBitMapRegionAddress = (ULONG_PTR *)kmalloc(PAGE_SIZE);
	m_VmStackRootRegionPointer = (ULONG_PTR)kmalloc(PAGE_SIZE * 10);

	if (!m_VmOnRegionAddress || !m_VmCsRegionAddress || !m_VmBitMapRegionAddress || !m_VmStackRootRegionPointer)
	{
		VtVmmMemFree();
		return FALSE;
	}

	m_VmOnRegionPhyAddress = MmGetPhysicalAddress(m_VmOnRegionAddress).QuadPart;
	m_VmCsRegionPhyAddress = MmGetPhysicalAddress(m_VmCsRegionAddress).QuadPart;
	m_VmMsrBitMapRegionPhyAddress = MmGetPhysicalAddress(m_VmBitMapRegionAddress).QuadPart;

	if (!m_VmOnRegionPhyAddress || !m_VmCsRegionPhyAddress || !m_VmMsrBitMapRegionPhyAddress)
	{
		VtVmmMemFree();
		return FALSE;
	}

	kprint(("Cpu:[%d] Vt �ڴ��ʼ���ɹ�!\r\n", m_CpuNumber));

	return TRUE;
}

// �ͷ� VMM �ڴ�
VOID VtBase::VtVmmMemFree()
{
	if (m_VmOnRegionAddress)
	{
		kFree(m_VmOnRegionAddress);
		m_VmOnRegionAddress = NULL;
	}

	if (m_VmCsRegionAddress)
	{
		kFree(m_VmCsRegionAddress);
		m_VmCsRegionAddress = NULL;
	}

	if (m_VmBitMapRegionAddress)
	{
		kFree(m_VmBitMapRegionAddress);
		m_VmBitMapRegionAddress = NULL;
	}

	if (m_VmStackRootRegionPointer)
	{
		kFree((PVOID)m_VmStackRootRegionPointer);
		m_VmStackRootRegionPointer = NULL;
	}
}

// ���� VMCS ����
BOOLEAN VtBase::SetupVmcs()
{
	if (m_VmxOn)
	{
		kprint(("[+]Cpu:[%d] ���⻯���������С�!\n", m_CpuNumber));
		return FALSE;
	}

	BOOLEAN retbool = TRUE;
	
	kprint(("Cpu:[%d] ����VMCS����\r\n", m_CpuNumber));

	// 2. ��ʼ�� Guest �� Host ����
	KdBreakPoint();
	// (1). ���� Guest ״̬
	m_GuestState.cs = __readcs();
	m_GuestState.ds = __readds();
	m_GuestState.ss = __readss();
	m_GuestState.es = __reades();
	m_GuestState.fs = __readfs();
	m_GuestState.gs = __readgs();

	m_GuestState.ldtr = __sldt();
	m_GuestState.tr = __str();
	m_GuestState.rflags = __readeflags();

	m_GuestState.rsp = GuestRsp; // ���� GUEST �� RSP��RIP
	m_GuestState.rip = GuestRip; 

	__sgdt(&(m_GuestState.gdt));
	__sidt(&(m_GuestState.idt));

	m_GuestState.cr3 = __readcr3();
	m_GuestState.cr0 = ((__readcr0() & __readmsr(IA32_VMX_CR0_FIXED1)) | __readmsr(IA32_VMX_CR0_FIXED0));
	m_GuestState.cr4 = ((__readcr4() & __readmsr(IA32_VMX_CR4_FIXED1)) | __readmsr(IA32_VMX_CR4_FIXED0));

	m_GuestState.dr7 = __readdr(7);
	m_GuestState.msr_debugctl = __readmsr(IA32_DEBUGCTL);
	m_GuestState.msr_sysenter_cs = __readmsr(IA32_SYSENTER_CS);
	m_GuestState.msr_sysenter_eip = __readmsr(IA32_SYSENTER_EIP);
	m_GuestState.msr_sysenter_esp = __readmsr(IA32_SYSENTER_ESP);

	__writecr0(m_GuestState.cr0);
	__writecr4(m_GuestState.cr4);

	m_GuestState.msr_efer = __readmsr(MSR_IA32_EFER); // ��� Guest EFER

	// (2). ��ʼ�� Host ״̬
	m_HostState.cr0 = __readcr0();
	m_HostState.cr3 = __readcr3();
	m_HostState.cr4 = __readcr4();

	m_HostState.cs = __readcs() & 0xF8;
	m_HostState.ds = __readds() & 0xF8;
	m_HostState.ss = __readss() & 0xF8;
	m_HostState.es = __reades() & 0xF8;
	m_HostState.fs = __readfs() & 0xF8;
	m_HostState.gs = __readgs() & 0xF8;
	m_HostState.tr = __str();

	m_HostState.rsp = ROUNDUP((m_VmStackRootRegionPointer + 0x2000), PAGE_SIZE); // ���� HOST �� RSP��RIP
	m_HostState.rip = reinterpret_cast<ULONG_PTR>(Asm_VmExitHandler);

	m_HostState.msr_sysenter_cs = __readmsr(IA32_SYSENTER_CS);
	m_HostState.msr_sysenter_eip = __readmsr(IA32_SYSENTER_EIP);
	m_HostState.msr_sysenter_esp = __readmsr(IA32_SYSENTER_ESP);

	m_HostState.msr_efer = __readmsr(MSR_IA32_EFER); // ��� Host EFER

	__sgdt(&(m_HostState.gdt));
	__sidt(&(m_HostState.idt));

	// 3. Setup Vmx
	// ������ VMCS ����
	retbool = InitVmcs();
	if (!retbool) return retbool;

	// 4. ��д VMCS �е� GUEST ״̬
	// (1). CS\SS\DS\ES\FS\GS\TR �Ĵ���
	ULONG_PTR uBase, uLimit, uAccess;
	GetSelectorInfoBySelector(m_GuestState.cs, &uBase, &uLimit, &uAccess);
	retbool &= VmCsWrite(GUEST_CS_SELECTOR, m_GuestState.cs);
	retbool &= VmCsWrite(GUEST_CS_LIMIT, uLimit);
	retbool &= VmCsWrite(GUEST_CS_AR_BYTES, uAccess);
	retbool &= VmCsWrite(GUEST_CS_BASE, uBase);

	GetSelectorInfoBySelector(m_GuestState.ss, &uBase, &uLimit, &uAccess);
	retbool &= VmCsWrite(GUEST_SS_SELECTOR, m_GuestState.ss);
	retbool &= VmCsWrite(GUEST_SS_LIMIT, uLimit);
	retbool &= VmCsWrite(GUEST_SS_AR_BYTES, uAccess);
	retbool &= VmCsWrite(GUEST_SS_BASE, uBase);

	GetSelectorInfoBySelector(m_GuestState.ds, &uBase, &uLimit, &uAccess);
	retbool &= VmCsWrite(GUEST_DS_SELECTOR, m_GuestState.ds);
	retbool &= VmCsWrite(GUEST_DS_LIMIT, uLimit);
	retbool &= VmCsWrite(GUEST_DS_AR_BYTES, uAccess);
	retbool &= VmCsWrite(GUEST_DS_BASE, uBase);

	GetSelectorInfoBySelector(m_GuestState.es, &uBase, &uLimit, &uAccess);
	retbool &= VmCsWrite(GUEST_ES_SELECTOR, m_GuestState.es);
	retbool &= VmCsWrite(GUEST_ES_LIMIT, uLimit);
	retbool &= VmCsWrite(GUEST_ES_AR_BYTES, uAccess);
	retbool &= VmCsWrite(GUEST_ES_BASE, uBase);
	retbool &= VmCsWrite(HOST_ES_SELECTOR, m_HostState.es);

	GetSelectorInfoBySelector(m_GuestState.fs, &uBase, &uLimit, &uAccess);
	retbool &= VmCsWrite(GUEST_FS_SELECTOR, m_GuestState.fs);
	retbool &= VmCsWrite(GUEST_FS_LIMIT, uLimit);
	retbool &= VmCsWrite(GUEST_FS_AR_BYTES, uAccess);
	retbool &= VmCsWrite(GUEST_FS_BASE, uBase);
	m_HostState.fsbase = uBase;


	GetSelectorInfoBySelector(m_GuestState.gs, &uBase, &uLimit, &uAccess);
	retbool &= VmCsWrite(GUEST_GS_SELECTOR, m_GuestState.gs);
	uBase = __readmsr(MSR_GS_BASE);
	retbool &= VmCsWrite(GUEST_GS_LIMIT, uLimit);
	retbool &= VmCsWrite(GUEST_GS_AR_BYTES, uAccess);
	retbool &= VmCsWrite(GUEST_GS_BASE, uBase);
	m_HostState.gsbase = uBase;

	GetSelectorInfoBySelector(m_GuestState.tr, &uBase, &uLimit, &uAccess);
	retbool &= VmCsWrite(GUEST_TR_SELECTOR, m_GuestState.tr);
	retbool &= VmCsWrite(GUEST_TR_LIMIT, uLimit);
	retbool &= VmCsWrite(GUEST_TR_AR_BYTES, uAccess);
	retbool &= VmCsWrite(GUEST_TR_BASE, uBase);
	m_HostState.trbase = uBase;

	GetSelectorInfoBySelector(m_GuestState.ldtr, &uBase, &uLimit, &uAccess);
	retbool &= VmCsWrite(GUEST_LDTR_SELECTOR, m_GuestState.ldtr);
	retbool &= VmCsWrite(GUEST_LDTR_LIMIT, uLimit);
	retbool &= VmCsWrite(GUEST_LDTR_AR_BYTES, uAccess);
	retbool &= VmCsWrite(GUEST_LDTR_BASE, uBase);

	// (2). GDTR\IDTR ��Ϣ
	retbool &= VmCsWrite(GUEST_GDTR_BASE, m_GuestState.gdt.uBase);
	retbool &= VmCsWrite(GUEST_GDTR_LIMIT, m_GuestState.gdt.uLimit);

	retbool &= VmCsWrite(GUEST_IDTR_BASE, m_GuestState.idt.uBase);
	retbool &= VmCsWrite(GUEST_IDTR_LIMIT, m_GuestState.idt.uLimit);

	// (3). ���ƼĴ��� CR0\CR3\CR4
	retbool &= VmCsWrite(GUEST_CR0, m_GuestState.cr0);
	retbool &= VmCsWrite(GUEST_CR3, m_GuestState.cr3);
	retbool &= VmCsWrite(CR0_READ_SHADOW, m_GuestState.cr0);

	retbool &= VmCsWrite(GUEST_CR4, m_GuestState.cr4);
	retbool &= VmCsWrite(CR4_READ_SHADOW, m_GuestState.cr4);

	// (4). RSP\RIP �� RFLAGS��DR7
	retbool &= VmCsWrite(GUEST_IA32_DEBUGCTL, m_GuestState.msr_debugctl);
	retbool &= VmCsWrite(GUEST_DR7, m_GuestState.dr7);
	retbool &= VmCsWrite(GUEST_RSP, m_GuestState.rsp);
	retbool &= VmCsWrite(GUEST_RIP, m_GuestState.rip);
	retbool &= VmCsWrite(GUEST_RFLAGS, m_GuestState.rflags);

	retbool &= VmCsWrite(GUEST_EFER, m_GuestState.msr_efer);

	if (!retbool) return FALSE; // һ�����ɹ�����FALSE

	// 5. ��ʼ��������(HOST)״̬
	retbool &= VmCsWrite(HOST_CS_SELECTOR, m_HostState.cs);
	retbool &= VmCsWrite(HOST_SS_SELECTOR, m_HostState.ss);
	retbool &= VmCsWrite(HOST_DS_SELECTOR, m_HostState.ds);

	retbool &= VmCsWrite(HOST_FS_BASE, m_HostState.fsbase);
	retbool &= VmCsWrite(HOST_FS_SELECTOR, m_HostState.fs);

	retbool &= VmCsWrite(HOST_GS_BASE, m_HostState.gsbase);
	retbool &= VmCsWrite(HOST_GS_SELECTOR, m_HostState.gs);

	retbool &= VmCsWrite(HOST_TR_BASE, m_HostState.trbase);
	retbool &= VmCsWrite(HOST_TR_SELECTOR, m_HostState.tr);

	retbool &= VmCsWrite(HOST_GDTR_BASE, m_HostState.gdt.uBase);
	retbool &= VmCsWrite(HOST_IDTR_BASE, m_HostState.idt.uBase);

	retbool &= VmCsWrite(HOST_CR0, m_HostState.cr0);
	retbool &= VmCsWrite(HOST_CR4, m_HostState.cr4);
	retbool &= VmCsWrite(HOST_CR3, m_HostState.cr3);

	retbool &= VmCsWrite(HOST_RIP, m_HostState.rip);
	retbool &= VmCsWrite(HOST_RSP, m_HostState.rsp);

	retbool &= VmCsWrite(HOST_EFER, m_HostState.msr_efer);

	kprint(("[+]Cpu:[%d] ���� VMCS �������\n", m_CpuNumber));

	return retbool;
}

// �򻯲����� VMCS MSR ����
BOOLEAN VtBase::InitVmcs()
{
	// 3. Setup Vmx
	// ִ�� vmxon/vmclear/vmptrld ��ʼ�������� VMCS ����

	BOOLEAN isEnable = TRUE;
	isEnable &= ExecuteVmxOn();
	if (!isEnable)
	{
		kprint(("Cpu:[%d] VMXONʧ��!\n", m_CpuNumber));
		return FALSE;
	}
	
	// 1. ���û���pin��vmִ�п�����Ϣ�� ��Pin-Based VM-Execution Controls��
	Ia32VmxBasicMsr ia32basicMsr = { 0 };
	ia32basicMsr.all = __readmsr(MSR_IA32_VMX_BASIC);

	VmxPinBasedControls vm_pinctl_requested = { 0 }; // �������� Pin-Based VM-Execution Controls
	//vm_pinctl_requested.Bits.nmi_exiting = TRUE; // ���� Nmi �ж�
	// �μ���Ƥ�� (A-2 Vol. 3D)�������������⻯������(��2.5��)
	// ���IA32_VMX_BASIC MSR�е�λ55����ȡΪ1����ʹ�� MSR_IA32_VMX_TRUE_PINBASED_CTLS 
	VmxPinBasedControls vm_pinctl = {
		VmxAdjustControlValue((ia32basicMsr.Bits.vmx_capability_hint) ? MSR_IA32_VMX_TRUE_PINBASED_CTLS : MSR_IA32_VMX_PINBASED_CTLS,
								vm_pinctl_requested.all)};		    // ���� Pin-Based VM-Execution Controls ��ֵ
	isEnable &= VmCsWrite(PIN_BASED_VM_EXEC_CONTROL, vm_pinctl.all); // ���� Pin-Based VM-Execution Controls
	
	// 2. ���û��ڴ���������vmִ�п�����Ϣ�� ��Primary Processor-Based VM-Execution Controls��
	VmxProcessorBasedControls vm_procctl_requested = { 0 }; // �������� Primary Processor-Based VM-Execution Controls
	//vm_procctl_requested.Bits.cr3_load_exiting = TRUE;	// ����д��Cr3
	//vm_procctl_requested.Bits.cr3_store_exiting = TRUE;	// ���ض�ȡCr3
	vm_procctl_requested.Bits.use_msr_bitmaps = TRUE;		// ����MSR bitmap, ����msr�Ĵ�������,��������,��Ȼ�κη�msr�Ĳ������ᵼ��vmexit
	vm_procctl_requested.Bits.activate_secondary_control = TRUE; // ������չ�ֶ�
	VmxProcessorBasedControls vm_procctl = {
		VmxAdjustControlValue((ia32basicMsr.Bits.vmx_capability_hint) ? MSR_IA32_VMX_TRUE_PROCBASED_CTLS : MSR_IA32_VMX_PROCBASED_CTLS,
								vm_procctl_requested.all)}; // ���� Primary Processor-Based VM-Execution Controls ��ֵ
	isEnable &= VmCsWrite(CPU_BASED_VM_EXEC_CONTROL, vm_procctl.all);

	// 3. ���û��ڴ������ĸ���vmִ�п�����Ϣ�����չ�ֶ� ��Secondary Processor-Based VM-Execution Controls��
	VmxSecondaryProcessorBasedControls vm_procctl2_requested = { 0 };
	//vm_procctl2_requested.Bits.descriptor_table_exiting = TRUE;	// ���� LGDT, LIDT, LLDT, LTR, SGDT, SIDT, SLDT, STR.
	vm_procctl2_requested.Bits.enable_rdtscp = TRUE;		 // for Win10
	vm_procctl2_requested.Bits.enable_invpcid = TRUE;		 // for Win10
	vm_procctl2_requested.Bits.enable_xsaves_xstors = TRUE;	 // for Win10

	// ���￴�Ƿ���Ҫ���� EPT
	if (VtIsUseEpt) {
		vm_procctl2_requested.Bits.enable_ept = TRUE;  // ���� EPT
		vm_procctl2_requested.Bits.enable_vpid = TRUE; // ���� VPID
												// (VPID��������TLB��������, ����VPID�ο���ϵͳ���⻯:ԭ����ʵ�֡�(��139ҳ)����Ƥ��(Vol. 3C 28-1))
	}
	VmxSecondaryProcessorBasedControls vm_procctl2 = { VmxAdjustControlValue(
		MSR_IA32_VMX_PROCBASED_CTLS2, vm_procctl2_requested.all) };
	isEnable &= VmCsWrite(SECONDARY_VM_EXEC_CONTROL, vm_procctl2.all);

	// 4. ����vm-entry������
	VmxVmEntryControls vm_entryctl_requested = { 0 };
	vm_entryctl_requested.Bits.load_ia32_efer = TRUE;   // ���� EFER �Ĵ���
	vm_entryctl_requested.Bits.ia32e_mode_guest = TRUE; // 64ϵͳ������, �ο������������⻯������(��212ҳ)
	VmxVmEntryControls vm_entryctl = { VmxAdjustControlValue(
		(ia32basicMsr.Bits.vmx_capability_hint) ? MSR_IA32_VMX_TRUE_ENTRY_CTLS : MSR_IA32_VMX_ENTRY_CTLS,
		vm_entryctl_requested.all) };
	isEnable &= VmCsWrite(VM_ENTRY_CONTROLS, vm_entryctl.all);

	// 5. ����vm-exit������Ϣ��
	VmxVmExitControls vm_exitctl_requested = { 0 };
	vm_exitctl_requested.Bits.load_ia32_efer = TRUE;
	vm_exitctl_requested.Bits.save_ia32_efer = TRUE;
	vm_exitctl_requested.Bits.acknowledge_interrupt_on_exit = TRUE;
	vm_exitctl_requested.Bits.host_address_space_size = TRUE; // 64ϵͳ������, �ο������������⻯������(��219ҳ)
	VmxVmExitControls vm_exitctl = { VmxAdjustControlValue(
		(ia32basicMsr.Bits.vmx_capability_hint) ? MSR_IA32_VMX_TRUE_EXIT_CTLS : MSR_IA32_VMX_EXIT_CTLS,
		vm_exitctl_requested.all) };
	isEnable &= VmCsWrite(VM_EXIT_CONTROLS, vm_exitctl.all);

	/*
		��д MsrBitMap (�ο���Ƥ�� 24-14 Vol. 3C)
	*/

	// MSR - Read - BitMap
	PUCHAR msr_bit_map_read_low = (PUCHAR)m_VmBitMapRegionAddress;	// 0x00000000 - 0x00001FFF
	PUCHAR msr_bit_map_read_higt = msr_bit_map_read_low + 1024;		// 0xC0000000 - 0xC0001FFF
	RTL_BITMAP msr_bit_map_read_low_rtl = { 0 };
	RTL_BITMAP msr_bit_map_read_higt_rtl = { 0 };
	// ��ʼ��msr read bitmap������
	RtlInitializeBitMap(&msr_bit_map_read_low_rtl, (PULONG)msr_bit_map_read_low, 1024 * 8);
	RtlInitializeBitMap(&msr_bit_map_read_higt_rtl, (PULONG)msr_bit_map_read_higt, 1024 * 8);
	// ����msr read bitmap������
	RtlSetBit(&msr_bit_map_read_higt_rtl, MSR_LSTAR - 0xC0000000);     // MSR_LSTAR ֧�� syscall sysret
	RtlSetBit(&msr_bit_map_read_higt_rtl, MSR_IA32_EFER - 0xC0000000);	// ���� EFER ��д��, ���� VM-exit

	// MSR - Write - BitMap
	PUCHAR msr_bit_map_write_low = (PUCHAR)m_VmBitMapRegionAddress + 1024 * 2;	// 0x00000000 - 0x00001FFF
	PUCHAR msr_bit_map_write_higt = msr_bit_map_write_low + 1024;				// 0xC0000000 - 0xC0001FFF
	RTL_BITMAP msr_bit_map_write_low_rtl = { 0 };
	RTL_BITMAP msr_bit_map_write_higt_rtl = { 0 };
	// ��ʼ��msr write bitmap������
	RtlInitializeBitMap(&msr_bit_map_write_low_rtl, (PULONG)msr_bit_map_write_low, 1024 * 8);
	RtlInitializeBitMap(&msr_bit_map_write_higt_rtl, (PULONG)msr_bit_map_write_higt, 1024 * 8);
	// ����msr read bitmap������
	RtlSetBit(&msr_bit_map_read_higt_rtl, MSR_IA32_EFER - 0xC0000000);	// ���� EFER ��д��, ���� VM-exit

	 // VMX MSRs
	for (ULONG i = MSR_IA32_VMX_BASIC; i <= MSR_IA32_VMX_VMFUNC; i++)
		RtlSetBit(&msr_bit_map_read_low_rtl, i);
	isEnable &= VmCsWrite(MSR_BITMAP, m_VmMsrBitMapRegionPhyAddress);    // λͼ
	
	/*
		��д ExceptionBitMap
	*/
	ULONG_PTR exception_bitmap = 0;
	exception_bitmap |= (1 << InterruptionVector::EXCEPTION_VECTOR_INVALID_OPCODE); // �������� #UD �쳣(���� VM-exit)
	isEnable &= VmCsWrite(EXCEPTION_BITMAP, exception_bitmap);

	// VMCS link pointer �ֶγ�ʼ��
	ULONG_PTR link_pointer = 0xFFFFFFFFFFFFFFFFL;
	isEnable &= VmCsWrite(VMCS_LINK_POINTER, link_pointer);
	
	// Ept ��ʼ�������EPTP
	if (VtIsUseEpt)
	{
		isEnable &= VmCsWrite(EPT_POINTER, g_Ept->m_Eptp.all);	// ��д EptPointer	
		ULONG processor = KeGetCurrentProcessorNumberEx(NULL);
		isEnable &= VmCsWrite(VIRTUAL_PROCESSOR_ID, processor + 1); // VIRTUAL_PROCESSOR_ID��ֵΪCurrentProcessorNumber+0x1
	}

	return isEnable;
}

// ���� VT
// @return
BOOLEAN VtBase::VtEnable()
{
	kprint(("[+]Cpu:[%d] ֧�����⻯!\n", m_CpuNumber));
	
	// ���� VMON\VMCS �ڴ��ַ
	if (!VtVmmMemAllocate())
	{
		kprint(("Cpu:[%d] ���� VMON��VMCS �ڴ��ַERROR!\n", m_CpuNumber));
		return FALSE;
	}

	if (!m_VmxOn)
	{
		// û������ VT, ������ GUEST �� RSP\RIP
		kprint(("Cpu:[%d] û������ VT, ������ GUEST �� RSP��RIP!\n", m_CpuNumber));
		__GetStackPointer(&GuestRsp);
		__GetNextInstructionPointer(&GuestRip);
	}
	
	// ���� VMCS ����
	if (!SetupVmcs())
	{
		if (!m_VmxOn) // ���������Ϊ VT �Ѿ��������µĴ���
		{
			kprint(("Cpu:[%d] ���� VMCS ����ERROR!\r\n", m_CpuNumber));
		}
		return FALSE;
	}

	// ��ʼ�� VMCS ���
	kprint(("[+]Cpu:[%d] ׼���������⻯ing...\r\n", m_CpuNumber));
	
	__vmx_vmlaunch(); // �����仰ִ�гɹ�,�Ͳ��᷵��

	if (m_VmxOn)	 // ���˴����� VT ����ʧ��
	{
		size_t uError = 0;
		uError = VtBase::VmCsRead(VM_INSTRUCTION_ERROR); // �ο���Ƥ�� (Vol. 3C 30-29)
		kprint(("ERROR:[%d] vmlaunch ָ�����ʧ��!\r\n", uError));

		__vmx_off(); // �ر� CPU �� VMX ģʽ
		m_VmxOn = FALSE;
	}
	kprint(("Cpu:[%d] VT ���⻯ʧ��!\r\n", m_CpuNumber));

	__writeds(0x28 | 0x3); // RTL��Ϊ1
	__writees(0x28 | 0x3);
	__writefs(0x50 | 0x3);

	return TRUE;
}

// �ر� VT
// @return
BOOLEAN VtBase::VtClose()
{
	if (m_VmxOn)
	{
		// ͨ�� VMCALL �˳� VT
		Asm_VmxCall(CallExitVt);
		m_VmxOn = FALSE;

		// �ͷ� VMM �ڴ�
		VtVmmMemFree();

		VtIsUseEpt = FALSE;

		kprint(("Cpu:[%d] VT ��ж��!\r\n", m_CpuNumber));
	}

	// ����CR4
	Cr4 cr4 = { 0 };
	cr4.all = __readcr4();
	cr4.Bits.vmxe = FALSE; // CR4.VMXE ��Ϊ 0
	__writecr4(cr4.all);

	return TRUE;
}

