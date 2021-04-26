#include "VtVmExit.h"
#include "VtBase.h"
#include "VtEpt.h"
#include "VtEptHook.h"
#include "VtEvent.h"
#include "Tools.h"
#include "VtSsdtHook.h"

extern VtEpt * g_Ept;
EXTERN_C PVOID KiSystemCall64Pointer;

void ShowGuestRegister(ULONG_PTR* Registers)
{
	ULONG_PTR Rip = 0, Rsp = 0;
	ULONG_PTR Cr0 = 0, Cr3 = 0, Cr4 = 0;
	ULONG_PTR Cs = 0, Ss = 0, Ds = 0, Es = 0, Fs = 0, Gs = 0, Tr = 0, Ldtr = 0;
	ULONG_PTR GsBase = 0, DebugCtl = 0, Dr7 = 0, RFlags = 0;
	ULONG_PTR IdtBase = 0, GdtBase = 0, IdtLimit = 0, GdtLimit = 0;

	DbgPrint("Debug:RAX = 0x%016llX RCX = 0x%016llX RDX = 0x%016llX RBX = 0x%016llX\n",
		Registers[R_RAX], Registers[R_RCX], Registers[R_RDX], Registers[R_RBX]);
	DbgPrint("Debug:RSP = 0x%016llX RBP = 0x%016llX RSI = 0x%016llX RDI = 0x%016llX\n",
		Registers[R_RSP], Registers[R_RBP], Registers[R_RSI], Registers[R_RDI]);
	DbgPrint("Debug:R8 = 0x%016llX R9 = 0x%016llX R10 = 0x%016llX R11 = 0x%016llX\n",
		Registers[R_R8], Registers[R_R9], Registers[R_R10], Registers[R_R11]);
	DbgPrint("Debug:R12 = 0x%016llX R13 = 0x%016llX R14 = 0x%016llX R15 = 0x%016llX\n",
		Registers[R_R12], Registers[R_R13], Registers[R_R14], Registers[R_R15]);
	DbgPrint("\r\n");

	__vmx_vmread(GUEST_RSP, &Rsp);
	__vmx_vmread(GUEST_RIP, &Rip);
	DbgPrint("Debug:RSP = 0x%016llX RIP = 0x%016llX\n", Rsp, Rip);

	__vmx_vmread(GUEST_CR0, &Cr0);
	__vmx_vmread(GUEST_CR3, &Cr3);
	__vmx_vmread(GUEST_CR4, &Cr4);
	DbgPrint("Debug:CR0 = 0x%016llX CR3 = 0x%016llX CR4 = 0x%016llX\n", Cr0, Cr3, Cr4);

	__vmx_vmread(GUEST_CS_SELECTOR, &Cs);
	__vmx_vmread(GUEST_SS_SELECTOR, &Ss);
	__vmx_vmread(GUEST_DS_SELECTOR, &Ds);
	__vmx_vmread(GUEST_ES_SELECTOR, &Es);
	__vmx_vmread(GUEST_FS_SELECTOR, &Fs);
	__vmx_vmread(GUEST_GS_SELECTOR, &Gs);
	__vmx_vmread(GUEST_TR_SELECTOR, &Tr);
	__vmx_vmread(GUEST_LDTR_SELECTOR, &Ldtr);
	DbgPrint("Debug:CS = 0x%016llX SS = 0x%016llX DS = 0x%016llX ES = 0x%016llX FS = 0x%016llX GS = 0x%016llX TR = 0x%016llX LDTR = 0x%016llX\n",
		Cs, Ss, Ds, Es, Fs, Gs, Tr, Ldtr);

	__vmx_vmread(GUEST_GS_BASE, &GsBase);
	__vmx_vmread(GUEST_IA32_DEBUGCTL, &DebugCtl);
	__vmx_vmread(GUEST_DR7, &Dr7);
	__vmx_vmread(GUEST_RFLAGS, &RFlags);
	DbgPrint("Debug:GsBase = 0x%016llX DebugCtl = 0x%016llX Dr7 = 0x%016llX RFlags = 0x%016llX\n",
		GsBase, DebugCtl, Dr7, RFlags);

	__vmx_vmread(GUEST_IDTR_BASE, &IdtBase);
	__vmx_vmread(GUEST_IDTR_LIMIT, &IdtLimit);
	DbgPrint("Debug:IdtBase = 0x%016llX IdtLimit = 0x%016llX\n", IdtBase, IdtLimit);

	__vmx_vmread(GUEST_GDTR_BASE, &GdtBase);
	__vmx_vmread(GUEST_GDTR_LIMIT, &GdtLimit);
	DbgPrint("Debug:GdtBase = 0x%016llX GdtLimit = 0x%016llX\n", GdtBase, GdtLimit);

	return VOID();
}

// ����ͳһ���� VM-EXIT
EXTERN_C FASTCALL
VOID VtVmExitRoutine(ULONG_PTR * Registers)
{
	KIRQL irql = KeGetCurrentIrql();
	if (irql < DISPATCH_LEVEL) {
		KeRaiseIrqlToDpcLevel(); // ���� IRQL �ȼ�Ϊ DPC_LEVEL
	}
	
	VmExitInformation exitReason = { 0 };
	FlagRegister guestRflag = { 0 };

	exitReason.all = (ULONG32)VtBase::VmCsRead(VM_EXIT_REASON); // ��ȡ VM-exit ԭ��

	switch (exitReason.Bits.reason)
	{
	case ExitExceptionOrNmi:	// ���� Nmi �ж�(��������)
		NmiExceptionVtExitHandler(Registers);
		break;
	case ExitExternalInterrupt: // �����ⲿ�ж�(������)
		ExternalInterruptVtExitHandler(Registers);
		break;
	case ExitCpuid:			// ���� cpuid
		CpuidVmExitHandler(Registers);
		break;
	case ExitVmcall:		// ���� vmcall
		VmcallVmExitHandler(Registers);
		break;
	case ExitCrAccess:		// ���ط��� CrX �Ĵ���
		CrAccessVtExitHandler(Registers);
		break;
	case ExitMsrRead:		// ����msr�Ĵ�������,��������,��Ȼ�κη�msr�Ĳ������ᵼ��vmexit
		MsrReadVtExitHandler(Registers);
		break;
	case ExitMsrWrite:		// ����msr�Ĵ��� д��
		MsrWriteVtExitHandler(Registers);
		break;
	case ExitGdtrOrIdtrAccess:	// ���� LGDT��LIDT��SGDT or SIDT ָ��
		GdtrOrIdtrAccessVtExitHandler(Registers);
		break;
	case ExitLdtrOrTrAccess:	// ���� LLDT, LTR, SLDT, or STR ָ��
		LdtrOrTrAccessVtExitHandler(Registers);
	case ExitEptViolation:	// EPT Violation ���µ� VM-EXIT
		g_Ept->EptViolationVtExitHandler(Registers);
		break;
	case ExitEptMisconfig:	// Ept ���ô���
		kprint(("ExitEptMisconfig!\r\n"));
		DbgBreakPoint();
		break;
	case ExitTripleFault:	// 3���쳣,�����Ĵ���ֱ������;
		kprint(("ExitTripleFault 0x%p!\r\n", VtBase::VmCsRead(GUEST_RIP)));
		DbgBreakPoint();
		break;
	case ExitXsetbv:		// Win10 ���봦����ٻ���
		_xsetbv((ULONG32)Registers[R_RCX], MAKEQWORD(Registers[R_RAX], Registers[R_RDX]));
		break;
	case ExitInvd:
		__wbinvd();
		break;
	case ExitVmclear:		// �ܾ� VT Ƕ��
	case ExitVmptrld:
	case ExitVmptrst:
	case ExitVmread:
	case ExitVmwrite:
	case ExitVmresume:
	case ExitVmoff:
	case ExitVmon:
	case ExitVmlaunch:
	case ExitVmfunc:
	case ExitInvept:
	case ExitInvvpid:
	{
		// ���� rflags �� cf λ, ��Ϊ1(����ʧ��)
		guestRflag.all = VtBase::VmCsRead(GUEST_RFLAGS);
		guestRflag.Bits.cf = 1;
		VtBase::VmCsWrite(GUEST_RFLAGS, guestRflag.all);
		// ��Ĭ������
		DefaultVmExitHandler(Registers);
	}
		break;
	default:		// Ĭ������
		DefaultVmExitHandler(Registers);
		kprint(("[+]default: δ֪�� VM_EIXT ԭ��:0x%X\n", exitReason));
		break;
	}

	if (irql < DISPATCH_LEVEL) {
		KeLowerIrql(irql);
	}
	
	return VOID();
}

// ���ڴ��� CPUID VM-EXIT
EXTERN_C
VOID CpuidVmExitHandler(ULONG_PTR * Registers)
{
	int CpuInfo[4] = { 0 };

	if (Registers[R_RAX] == 0x88888888)
	{
		//KdBreakPoint();

		Registers[R_RAX] = 0x88888888;
		Registers[R_RBX] = 0x88888888;
		Registers[R_RCX] = 0x88888888;
		Registers[R_RDX] = 0x88888888;
	}
	else
	{
		// Ĭ����������
		__cpuidex(CpuInfo, (int)Registers[R_RAX], (int)Registers[R_RCX]);
		Registers[R_RAX] = (ULONG_PTR)CpuInfo[0];
		Registers[R_RBX] = (ULONG_PTR)CpuInfo[1];
		Registers[R_RCX] = (ULONG_PTR)CpuInfo[2];
		Registers[R_RDX] = (ULONG_PTR)CpuInfo[3];
	}

	// ��Ĭ������
	DefaultVmExitHandler(Registers);

	return VOID();
}

// ���ڴ��� CrX VM-EXIT
EXTERN_C
VOID CrAccessVtExitHandler(ULONG_PTR * Registers)
{
	CrxVmExitQualification CrxQualification = { 0 };
	CrxQualification.all = VtBase::VmCsRead(EXIT_QUALIFICATION); // ��ȡ�ֶ���Ϣ

	if (CrxQualification.Bits.lmsw_operand_type == 0)
	{
		switch (CrxQualification.Bits.crn)
		{
		case 3: // ���� Cr3
		{
			if (CrxQualification.Bits.access_type == MovCrAccessType::KMobeFromCr)   // MOV reg,cr3
			{
				Registers[CrxQualification.Bits.gp_register] = VtBase::VmCsRead(GUEST_CR3);
			}
			else if(CrxQualification.Bits.access_type == kMoveToCr) // MOV crx, reg 
			{
				VtBase::VmCsWrite(GUEST_CR3, Registers[CrxQualification.Bits.gp_register]);
			}
		}
		break;
		default:
			kprint(("CrAccessVtExitHandler: ����Cr[%d]!\r\n", CrxQualification.Bits.crn));
			break;
		}
	}

	// ��Ĭ������
	DefaultVmExitHandler(Registers);

	return VOID();
}

// ���ڴ��� VMCALL VM-EXIT
EXTERN_C
VOID VmcallVmExitHandler(ULONG_PTR * Registers)
{
	ULONG_PTR JmpEIP = 0;
	ULONG_PTR GuestRIP = 0, GuestRSP = 0;
	ULONG_PTR ExitInstructionLength = 0;

	GuestRIP = VtBase::VmCsRead(GUEST_RIP);
	GuestRSP = VtBase::VmCsRead(GUEST_RSP);
	ExitInstructionLength = VtBase::VmCsRead(VM_EXIT_INSTRUCTION_LEN);

	switch (Registers[R_RAX])
	{
	case CallSsdtHook:
	{
		VtSsdtHook::VtEferHook();
	}
		break;
	case CallEptHook:	// �ṩ hook �ķ�ʽ
	{
		//KdBreakPoint();
		PVOID retaddr = VtEptHook::VtEptHookMemory(Registers[R_RDX], Registers[R_R8], 1);
		*(PVOID *)Registers[R_R9] = retaddr; // ����ԭ��������
	}
		break;
	case CallDelEptHook: // �ṩ hook ж�صķ�ʽ
		break;
	case CallExitVt: // �˳���ǰ���⻯
	{
		DbgPrint("Debug:��Over VMCALL�����á�\n");

		__vmx_off(); // �˳���ǰ���⻯

		JmpEIP = GuestRIP + ExitInstructionLength; // Խ������ VM-EXIT ��ָ��
		// �޸� Rsp\Rip ���ص� Guest ��
		Asm_UpdateRspAndRip(GuestRSP, JmpEIP);
	}
	break;
	default:
		break;
	}

	// ��Ĭ������
	DefaultVmExitHandler(Registers);

	return VOID();
}

// �����ȡ MSR VM-EXIT
EXTERN_C
VOID MsrReadVtExitHandler(ULONG_PTR * Registers)
{
	ULONGLONG MsrValue = __readmsr((ULONG)Registers[R_RCX]);
	
	switch (Registers[R_RCX])
	{
	case MSR_LSTAR: // ��ȡ MSR RIP
	{
		KdBreakPoint();
		if (KiSystemCall64Pointer) {
			MsrValue = (ULONG_PTR)KiSystemCall64Pointer; // SSDT HOOK
		}
	}
	default:
	{
		// Ĭ����������
		Registers[R_RAX] = LODWORD(MsrValue);
		Registers[R_RDX] = HIDWORD(MsrValue);
	}
	break;
	}

	// ��Ĭ������
	DefaultVmExitHandler(Registers);

	return VOID();
}

// ����д�� MSR VM-EXIT
EXTERN_C
VOID MsrWriteVtExitHandler(ULONG_PTR * Registers)
{
	ULONGLONG MsrValue = MAKEQWORD(Registers[R_RAX], Registers[R_RDX]);

	switch (Registers[R_RCX])
	{
	case IA32_SYSENTER_EIP: // д�� MSR 0x176
	case IA32_SYSENTER_ESP: // д�� MSR 0x175
	case IA32_SYSENTER_CS:	// д�� MSR 0x174
	default:
	{
		// Ĭ����������
		__writemsr((ULONG)Registers[R_RCX], MsrValue);
	}
	break;
	}

	// ��Ĭ������
	DefaultVmExitHandler(Registers);

	return VOID();
}

// ���ڴ��� Nmi �ж� (���������ж�)
EXTERN_C
VOID NmiExceptionVtExitHandler(ULONG_PTR * Registers)
{
	UNREFERENCED_PARAMETER(Registers);

	VmxVmExit_Interrupt_info exception = { 0 }; // ����ֱ�������¼�
	InterruptionType interruption_type = InterruptionType::kExternalInterrupt; // Ĭ�ϳ�ʼ��
	InterruptionVector vector = InterruptionVector::EXCEPTION_VECTOR_DIVIDE_ERROR;
	ULONG32 error_code_valid = 0;

	/*
		"ֱ�������¼�" ��ֱָ������ VM-exit �������¼��������������֣�
		(1). Ӳ���쳣�������쳣���������� exception bitmap ��Ӧ��λΪ1��ֱ�ӵ��� VM-exit.
		(2). ����쳣(#BP��#OF)�������쳣���������� exception bitmap ��Ӧ��λΪ1��ֱ�ӵ��� VM-exit.
		(3). �ⲿ�жϣ������ⲿ�ж�����ʱ, ����"exception-interrupt exiting"Ϊ1��ֱ�ӵ��� VM-exit.
		(4). NMI������NMI����ʱ, ����"NMI exiting"Ϊ1��ֱ�ӵ��� VM-exit.
	*/

	// �����ж�ʱ, ��ȡ VM-Exit Interruption-Information �ֶ�
	exception.all = static_cast<ULONG32>(VtBase::VmCsRead(VM_EXIT_INTR_INFO));

	interruption_type = static_cast<InterruptionType>(exception.Bits.interruption_type); // ��ȡ�ж�����
	vector = static_cast<InterruptionVector>(exception.Bits.vector); // ��ȡ�ж�������
	error_code_valid = exception.Bits.error_code_valid; // �Ƿ��д�����

	if (interruption_type == InterruptionType::kHardwareException)
	{
		// �����Ӳ���쳣, ����������ڴ���쳣
		if (vector == InterruptionVector::EXCEPTION_VECTOR_PAGE_FAULT)
		{
			// ���Ϊ #PF �쳣
			// exit qualification �ֶδ洢���� #PF �쳣�����Ե�ֵַ (�ο������������⻯������(��3.10.1.6��))
			auto fault_address = VtBase::VmCsRead(EXIT_QUALIFICATION);

			// VM-exit interruption error code �ֶ�ָ����� Page-Fault Error Code (�ο������������⻯������(��3.10.2��))
			PageFaultErrorCode fault_code = { 0 };
			fault_code.all = static_cast<ULONG32>(VtBase::VmCsRead(VM_EXIT_INTR_ERROR_CODE));

			// Ĭ�ϲ��޸ģ�����ע���ȥ
			VtEvent::VtInjectInterruption(interruption_type, vector, true, fault_code.all);

			//kprint(("[+] #PF �쳣!\r\n"));

			// ע��ͬ�� cr2 �Ĵ���
			__writecr2(fault_address);

			VtBase::VmCsWrite(VM_ENTRY_INTR_INFO, exception.all);

			if (error_code_valid) {
				VtBase::VmCsWrite(VM_ENTRY_EXCEPTION_ERROR_CODE, VtBase::VmCsRead(VM_EXIT_INTR_ERROR_CODE));
			}

		}
		else if (vector == InterruptionVector::EXCEPTION_VECTOR_GENERAL_PROTECTION){
			// ���Ϊ #GP �쳣

			auto error_code = VtBase::VmCsRead(VM_EXIT_INTR_ERROR_CODE);

			// Ĭ�ϲ��޸ģ�����ע���ȥ
			VtEvent::VtInjectInterruption(interruption_type, vector, true, (ULONG32)error_code);

			//kprint(("[+] #GP �쳣!\r\n"));
		}
		else if (vector == InterruptionVector::EXCEPTION_VECTOR_INVALID_OPCODE) {
			// ����� #UD �쳣

			/*
				�ж��Ƿ�Ϊ SysCall/SysRet ָ��
			*/
			if (!VtSsdtHook::UdExceptionVtExitHandler(Registers)) {

				/*
					�������Ĭ��ע�� #UD
				*/
				VtEvent::VtInjectInterruption(interruption_type, vector, false, 0);
			}
		}
	}
	else if (interruption_type == InterruptionType::kSoftwareException) {
		// ����� ����쳣
		if (vector == InterruptionVector::EXCEPTION_VECTOR_BREAKPOINT)
		{
			// #BP
			// int3 ����������쳣, ע���ָ���г���
			// Ĭ�ϲ��޸ģ�����ע���ȥ
			VtEvent::VtInjectInterruption(interruption_type, vector, false, 0);
			auto exit_inst_length = VtBase::VmCsRead(VM_EXIT_INSTRUCTION_LEN); // ��ȡ���� VM-exit ��ָ���
			VtBase::VmCsWrite(VM_ENTRY_INSTRUCTION_LEN, exit_inst_length);

			//kprint(("[+] #BP �쳣!\r\n"));
		}
	}
	else {
		//kprint(("[+] interruption_type:[%d]; vector:[%d] �쳣!\r\n", interruption_type, vector));
		VtBase::VmCsWrite(VM_ENTRY_INTR_INFO, exception.all);

		if (error_code_valid) {
			VtBase::VmCsWrite(VM_ENTRY_EXCEPTION_ERROR_CODE, VtBase::VmCsRead(VM_EXIT_INTR_ERROR_CODE));
		}
	}
}

// ���ڴ��� �ⲿ�ж�
EXTERN_C
VOID ExternalInterruptVtExitHandler(ULONG_PTR * Registers)
{
	DefaultVmExitHandler(Registers);
}

// ����� GDT/IDT ���ʵ��µ� VM-exit
EXTERN_C
VOID GdtrOrIdtrAccessVtExitHandler(ULONG_PTR * Registers)
{
	UNREFERENCED_PARAMETER(Registers);
	
	/*
		sgdt [eax + 4 * ebx + 0xC]
		eax : base_register
		ebx : index_register
		4	: scalling
		0xC	: displacement
	*/

	// ��ȡ��ƫ���� (�ο������������⻯������(��3.10.1.4��))
	ULONG_PTR displacement = VtBase::VmCsRead(EXIT_QUALIFICATION);
	// ��ȡָ�������Ϣ
	GdtrOrIdtrInstInformation instruction_info = { 0 };
	instruction_info.all = static_cast<ULONG32>(VtBase::VmCsRead(VMX_INSTRUCTION_INFO));

	ULONG_PTR scalling = static_cast<ULONG_PTR>(instruction_info.Bits.scalling);
	ULONG_PTR address_size = static_cast<ULONG_PTR>(instruction_info.Bits.address_size);
	ULONG_PTR operand_size = static_cast<ULONG_PTR>(instruction_info.Bits.operand_size);
	ULONG_PTR segment_register = static_cast<ULONG_PTR>(instruction_info.Bits.segment_register);
	ULONG_PTR index_register = static_cast<ULONG_PTR>(instruction_info.Bits.index_register);
	ULONG_PTR index_register_invalid = static_cast<ULONG_PTR>(instruction_info.Bits.index_register_invalid);
	ULONG_PTR base_register = static_cast<ULONG_PTR>(instruction_info.Bits.base_register);
	ULONG_PTR base_register_invalid = static_cast<ULONG_PTR>(instruction_info.Bits.base_register_invalid);
	ULONG_PTR instruction_identity = static_cast<ULONG_PTR>(instruction_info.Bits.instruction_identity);

	UNREFERENCED_PARAMETER(address_size);
	UNREFERENCED_PARAMETER(operand_size);

	ULONG_PTR base_address = 0;
	ULONG_PTR index_address = 0;
	ULONG_PTR total_address = 0;

	if (segment_register > 5) {
		//kprint(("������ѡ���ӽ���!\r\n"));
		return VOID();
	}

	if (!index_register_invalid && (index_register < 16)) {
		// ����в������Ĵ���, �ж��Ƿ��в�����
		if (scalling) index_address = 0;
		else {
			// �в�����
			index_address = Registers[index_register] << scalling;
		}
	}

	if (!base_register_invalid && (base_register < 16)) {
		// ����л�ַ�Ĵ���
		base_address = Registers[base_register];
	}

	// �����ڴ��ܴ�С (ע��˵�ַ�п����� ring3 ģʽ�µ�)
	total_address = VtBase::VmCsRead(GUEST_ES_BASE + segment_register * 2) + base_address + index_address + displacement;
	// ����һ�������ַ����ҳ����ӳ��һ�ݵ� ring0
	
	// �жϷ��ʵ�ָ������ ��0-SGDT��1-SIDT��2-LGDT��3-LIDT��
	switch (instruction_identity)
	{
	case 0:		// SGDT
	case 1:		// SIDT
	{
		ULONG_PTR cr3 = 0;
		PCHAR address = NULL;
		cr3 = VtBase::VmCsRead(GUEST_CR3);
		address = reinterpret_cast<PCHAR>(GetKernelModeLinerAddress(cr3, total_address));
		*(PULONG)(address + 2) = 0x12345678;
		*(PSHORT)(address) = 0x3ff;
		FreeKernelModeLinerAddress(address);
	}
	break;
	case 2:		// LGDT
		break;
	default:	// LIDT
		break;
	}

	// ��Ĭ������
	DefaultVmExitHandler(Registers);
	return VOID();
}

// ����� LDT/TR ���ʵ��µ� VM-exit
EXTERN_C
VOID LdtrOrTrAccessVtExitHandler(ULONG_PTR * Registers)
{
	UNREFERENCED_PARAMETER(Registers);
	//KdBreakPoint();
	/*

	*/

	// ��ȡ��ƫ���� (�ο������������⻯������(��3.10.1.4��))
	ULONG_PTR displacement = VtBase::VmCsRead(EXIT_QUALIFICATION);
	// ��ȡָ�������Ϣ
	LdtrOrTrInstInformation instruction_info = { 0 };
	instruction_info.all = static_cast<ULONG32>(VtBase::VmCsRead(VMX_INSTRUCTION_INFO));

	ULONG_PTR scalling = static_cast<ULONG_PTR>(instruction_info.Bits.scalling);
	ULONG_PTR register1 = static_cast<ULONG_PTR>(instruction_info.Bits.register1);
	ULONG_PTR address_size = static_cast<ULONG_PTR>(instruction_info.Bits.address_size);
	ULONG_PTR register_access = static_cast<ULONG_PTR>(instruction_info.Bits.register_access);
	ULONG_PTR segment_register = static_cast<ULONG_PTR>(instruction_info.Bits.segment_register);
	ULONG_PTR index_register = static_cast<ULONG_PTR>(instruction_info.Bits.index_register);
	ULONG_PTR index_register_invalid = static_cast<ULONG_PTR>(instruction_info.Bits.index_register_invalid);
	ULONG_PTR base_register = static_cast<ULONG_PTR>(instruction_info.Bits.base_register);
	ULONG_PTR base_register_invalid = static_cast<ULONG_PTR>(instruction_info.Bits.base_register_invalid);
	ULONG_PTR instruction_identity = static_cast<ULONG_PTR>(instruction_info.Bits.instruction_identity);

	UNREFERENCED_PARAMETER(address_size);
	UNREFERENCED_PARAMETER(register1);
	
	// ���жϷ�����ʽ
	if (register_access) {
		// ����ǼĴ����ķ�����ʽ
		// �жϷ���ָ������
		switch (instruction_identity) //��0-SLDT��1-STR��2-LLDT��3-LTR��
		{
		case 0: // SLDT
		{
			Registers[index_register] = VtBase::VmCsRead(GUEST_LDTR_SELECTOR);
		}
		break;
		case 1:	// STR
		{
			Registers[index_register] = VtBase::VmCsRead(GUEST_TR_SELECTOR);
		}
		break;
		case 2: // LLDT
		{
			VtBase::VmCsWrite(GUEST_LDTR_SELECTOR, Registers[index_register]);
		}
		break;
		case 3: // LTR
		{
			VtBase::VmCsWrite(GUEST_TR_SELECTOR, Registers[index_register]);
		}
		break;
		}
	}
	else {
		// ������ڴ�ķ�����ʽ
		if (segment_register > 5) {
			//kprint(("������ѡ���ӽ���!\r\n"));
			return VOID();
		}

		ULONG_PTR base_address = 0;
		ULONG_PTR index_address = 0;
		ULONG_PTR total_address = 0;

		if (!index_register_invalid && (index_register < 16)) {
			// ����в������Ĵ���, �ж��Ƿ��в�����
			if (scalling) index_address = 0;
			else {
				// �в�����
				index_address = Registers[index_register] << scalling;
			}
		}

		if (!base_register_invalid && (base_register < 16)) {
			// ����л�ַ�Ĵ���
			base_address = Registers[base_register];
		}

		// �����ڴ��ܴ�С (ע��˵�ַ�п����� ring3 ģʽ�µ�)
		total_address = VtBase::VmCsRead(GUEST_ES_BASE + segment_register * 2) + base_address + index_address + displacement;

		switch (instruction_identity) //��0-SLDT��1-STR��2-LLDT��3-LTR��
		{
		case 0: // SLDT
			break;
		case 1:	// STR
			break;
		case 2: // LLDT
			break;
		case 3: // LTR
			break;
		}
	}

	// ��Ĭ������
	DefaultVmExitHandler(Registers);

	return VOID();
}

// ���ڴ���Ĭ�� VM-EXIT
EXTERN_C
VOID DefaultVmExitHandler(ULONG_PTR * Registers)
{
	//ULONG_PTR exitReason = VtBase::VmCsRead(VM_EXIT_REASON); // ��ȡ VM-exit ԭ��
	ULONG_PTR guestRip = VtBase::VmCsRead(GUEST_RIP);
	ULONG_PTR guestRsp = VtBase::VmCsRead(GUEST_RSP);
	ULONG_PTR exitInstructionLength = VtBase::VmCsRead(VM_EXIT_INSTRUCTION_LEN); // �˳���ָ���

	UNREFERENCED_PARAMETER(Registers);

	VtBase::VmCsWrite(GUEST_RIP, guestRip + exitInstructionLength);
	VtBase::VmCsWrite(GUEST_RSP, guestRsp);

	return VOID();
}
