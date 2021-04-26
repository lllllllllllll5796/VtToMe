#include "VtSsdtHook.h"
#include "Tools.h"
#include "NeedKernelFunc.h"
#include "VtAsm.h"
#include "vt_win7_kernel_code_asm.h"
#include "vt_win10_kernel_code_asm.h"
#include "VtBase.h"
#include "VtEvent.h"
#include <intrin.h>

typedef struct _NT_KPROCESS
{
	DISPATCHER_HEADER Header;
	LIST_ENTRY	ProfileListHead;
	ULONG64		DirectoryTableBase;		// ������� R0 Cr3
	LIST_ENTRY	ThreadListHead;
	UINT32		ProcessLock;
	UINT32		ProcessTimerDelay;
	ULONG64		DeepFreezeStartTime;
	CHAR		Know[0x340];
	ULONG64		UserDirectoryTableBase; // ������� R3 Cr3
}NT_KPROCESS, *PNT_KPROCESS;

// �Ƿ�Ϊ SysRet ָ��
#define IS_SYSRET_INSTRUCTION(Code) \
(*((PUINT8)(Code) + 0) == 0x48 && \
*((PUINT8)(Code) + 1) == 0x0F && \
*((PUINT8)(Code) + 2) == 0x07)

// �Ƿ�Ϊ SysCall ָ��
#define IS_SYSCALL_INSTRUCTION(Code) \
(*((PUINT8)(Code) + 0) == 0x0F && \
*((PUINT8)(Code) + 1) == 0x05)

EXTERN_C CHAR  g_hook_enabled[MAX_SYSCALL_INDEX] = { 0 };	// �Ƿ�Hook���±���
EXTERN_C CHAR  g_param_table[MAX_SYSCALL_INDEX]  = { 0 };	// ����������
EXTERN_C PVOID g_ssdt_table[MAX_SYSCALL_INDEX]	= { 0 };	// ssdt ��(ֱ�Ӵ洢������ַ)

EXTERN_C PVOID KiSystemCall64Pointer = NULL;
EXTERN_C PVOID KiSystemServiceCopyEndPointer = NULL;

ULONG_PTR g_r3_Cr3 = 0;
ULONG_PTR g_r0_Cr3 = 0;

VtSsdtHook::VtSsdtHook()
{}

VtSsdtHook::~VtSsdtHook()
{}

// ��ʼ������
bool VtSsdtHook::VtInitSsdtHook()
{
	__debugbreak();
	if (NULL == KiSystemCall64Pointer) {
		// ��ȡ KiSystemCall64 ��ַ
		KiSystemCall64Pointer = (PVOID)__readmsr(MSR_LSTAR); // System Call Rip
	}

	if (GetWindowsVersion() == WIN7) {
		// ����� Win7 ֱ�Ӳ��� Hook Msr lstar ��ʽ
		if (NULL == KiSystemServiceCopyEndPointer) {
			// ��ȡ KiSystemServiceCopyEnd
			char opcode[] = "\xf7\x05********\x0F\x85****\x41\xFF\xD2";
			KiSystemServiceCopyEndPointer = MmFindByCode(opcode, 19);
		}
	}
	else if (GetWindowsVersion() == WIN10) {
		// ����� Win10 ���� Efer Hook
		if (NULL == KiSystemServiceCopyEndPointer) {
			// ��ȡ KiSystemServiceCopyEnd
			char opcode[] = "\xf7\x05********\x0F\x85****\xf7\x05********\x0F\x85****\x49\x8B\xC2\xFF\xD0";
			KiSystemServiceCopyEndPointer = MmFindByCode(opcode, 0x25);
		}
	}
	

	if (!KiSystemCall64Pointer || !KiSystemServiceCopyEndPointer) 
		return false;
	return true;
}

// Hook ָ���±� SSDT ����
bool VtSsdtHook::VtHookSsdtByIndex(ULONG32 ssdt_index, PVOID hook_address, CHAR param_number)
{
	UNREFERENCED_PARAMETER(ssdt_index);

	LARGE_INTEGER timeOut;
	timeOut.QuadPart = -1 * 1000 * 1000; // 0.1���ӳټ���, �Է� VT δ����
	KeDelayExecutionThread(KernelMode, FALSE, &timeOut);

	g_hook_enabled[ssdt_index] = TRUE;			// Hook ��־����
	g_param_table[ssdt_index]  = param_number;	// ȷ����������
	g_ssdt_table[ssdt_index]   = hook_address;	// �޸��亯����ַ

	return true;
}

// Hook Msr Lstar �Ĵ���
bool VtSsdtHook::VtHookMsrLstar()
{
	RemovWP();
	// HOOK LSTAR
	__writemsr(MSR_LSTAR, (ULONG_PTR)Win7SysCallEntryPointer); // �޸� SysCall Rip ����
	
	UnRemovWP();
	return true;
}

// Un Hook Msr Lstar �Ĵ���
bool VtSsdtHook::VtUnHookMsrLstar()
{
	RemovWP();

	// HOOK LSTAR
	__writemsr(MSR_LSTAR, (ULONG_PTR)KiSystemCall64Pointer); // �޸� SysCall Rip ����

	UnRemovWP();
	return true;
}

// Efer Hook
bool VtSsdtHook::VtEferHook()
{
	/*
		����EFER HOOK����Ҫ�Ĳ���(�ο� https://revers.engineering/syscall-hooking-via-extended-feature-enable-register-efer/)
		1. Enable VMX
		2. ���� VM-entry �е� load_ia32_efer �ֶ�
		3. ���� VM-exit �е� save_ia32_efer �ֶ�
		4. ���� MSR-bitmap ������, д��Ͷ�ȡEFER MSRʱ�˳�
		5. ���� Exception-bitmap ���� #UD �쳣
		6. ���� VM-exit �е� load_ia32_efer �ֶ�
		7. ��� sce λ
		8. ���� SysCall �� SysRet ָ��µ� #UD �쳣
	*/
	
	Ia32VmxEfer ia32_efer = { 0 };
	ia32_efer.all = __readmsr(MSR_IA32_EFER);
	ia32_efer.Bits.sce = false;
	// ��� sce λ
	VtBase::VmCsWrite(GUEST_EFER, ia32_efer.all);

	return true;
}

// #UD �쳣����
bool VtSsdtHook::UdExceptionVtExitHandler(ULONG_PTR * Registers)
{
	// ��ȡ������Ϣ
	BOOLEAN retbool = FALSE;
	ULONG_PTR guestRip = VtBase::VmCsRead(GUEST_RIP);
	ULONG_PTR guest_cr3 = VtBase::VmCsRead(GUEST_CR3);
	//ULONG_PTR guest_liner_address = VtBase::VmCsRead(GUEST_LINEAR_ADDRESS);
	//ULONG_PTR guest_phy_address = VtBase::VmCsRead(GUEST_PHYSICAL_ADDRESS);
	ULONG_PTR exitInstructionLength = VtBase::VmCsRead(VM_EXIT_INSTRUCTION_LEN); // �˳���ָ���

	NT_KPROCESS * CurrentProcess = reinterpret_cast<PNT_KPROCESS>(PsGetCurrentProcess());
	ULONG_PTR current_process_kernel_cr3 = CurrentProcess->DirectoryTableBase;
	
	UNREFERENCED_PARAMETER(guest_cr3);
	
	__writecr3(current_process_kernel_cr3); // �л�Ϊ�ں� Guest Cr3

	PULONG_PTR pte = GetPteAddress((PVOID)guestRip);

	if (!MmIsAddressValid((PVOID)guestRip) || !(*pte & 0x1)) { // �ж� Rip ��ַ�����Ƿ�ɶ�
		// ������ע�� #PF �쳣
		
		__writecr2(guestRip); // ����ȱҳ��ַ

		PageFaultErrorCode fault_code = { 0 };
		VtEvent::VtInjectInterruption(
			InterruptionType::kHardwareException, 
			InterruptionVector::EXCEPTION_VECTOR_PAGE_FAULT,
			TRUE, fault_code.all);

		return TRUE;
	}
	
	PVOID user_liner_address = GetKernelModeLinerAddress(current_process_kernel_cr3, guestRip);

	if (exitInstructionLength == 0x2) { // �ж�ָ���
		if (IS_SYSCALL_INSTRUCTION(user_liner_address)) {
			/*
				����� SysCall ָ��
			*/
			VtSysCallEmulate(Registers);

			retbool = TRUE;
		}
	}
	
	if (exitInstructionLength == 0x3) {
		if (IS_SYSRET_INSTRUCTION(user_liner_address)) {
			/*
				����� SysRet ָ��
			*/
			VtSysRetEmulate(Registers);

			retbool = TRUE;
		}
	}

	FreeKernelModeLinerAddress(user_liner_address);

	return retbool;
}

// ģ�� SysCall ����
bool VtSsdtHook::VtSysCallEmulate(ULONG_PTR * Registers)
{
	// ��ȡ������Ϣ
	PNT_KPROCESS current_process = (PNT_KPROCESS)PsGetCurrentProcess();
	ULONG_PTR MsrValue = 0;
	BOOLEAN boolret = TRUE;
	ULONG_PTR guestRip = VtBase::VmCsRead(GUEST_RIP);
	//ULONG_PTR guestRsp = VtBase::VmCsRead(GUEST_RSP);
	ULONG_PTR GuestRflags = VtBase::VmCsRead(GUEST_RFLAGS);
	//ULONG_PTR guest_r3_cr3 = VtBase::VmCsRead(GUEST_CR3);
	ULONG_PTR exitInstructionLength = VtBase::VmCsRead(VM_EXIT_INSTRUCTION_LEN); // �˳���ָ���

	// �ο���Ƥ�� SYSCALL��Fast System Call

	/*
		a.	SysCall loading Rip From the IA32_LSTA MSR
		b.	SysCall ���� IA32_LSTA MSR ��ֵ�� Rip ��
	*/
	//MsrValue = __readmsr(MSR_LSTAR);
	// �����ǵ�����
	MsrValue = (ULONG_PTR)Win10_SysCallEntryPointer;
	boolret &= VtBase::VmCsWrite(GUEST_RIP, MsrValue);

	/*
		a.	After Saving the Adress of the instruction following SysCall into Rcx
		b.	SysCall �Ὣ��һ��ָ���ַ���浽 Rcx ��
	*/
	auto next_instruction = exitInstructionLength + guestRip;
	Registers[R_RCX] = next_instruction;

	/*
		a. Save RFLAGS into R11 and then mask RFLAGS using MSR_FMASK.
		b. ���� RFLAGS �� R11 �Ĵ�����, ����ʹ�� MSR_FMASK ��� RFLAGS ��Ӧ��ÿһλ
	*/
	MsrValue = __readmsr(MSR_IA32_FMASK);
	Registers[R_R11] = GuestRflags;
	GuestRflags &= ~(MsrValue | X86_FLAGS_RF);
	VtBase::VmCsWrite(GUEST_RFLAGS, GuestRflags);

	/*
		a. SYSCALL loads the CS and SS selectors with values derived from bits 47:32 of the IA32_STAR MSR.
		b. SysCall ���� CS��SS �μĴ�����ֵ������ IA32_STAR MSR �Ĵ����� 32:47 λ
	*/
	MsrValue = __readmsr(MSR_IA32_STAR);
	ULONG_PTR Cs = (UINT16)((MsrValue >> 32) & ~3);
	boolret &= VtBase::VmCsWrite(GUEST_CS_SELECTOR, Cs);
	boolret &= VtBase::VmCsWrite(GUEST_CS_LIMIT, (UINT32)~0);
	boolret &= VtBase::VmCsWrite(GUEST_CS_AR_BYTES, 0xA09B);
	boolret &= VtBase::VmCsWrite(GUEST_CS_BASE, 0);

	ULONG_PTR Ss = Cs + 0x8;
	boolret &= VtBase::VmCsWrite(GUEST_SS_SELECTOR, Ss);
	boolret &= VtBase::VmCsWrite(GUEST_SS_LIMIT, (UINT32)~0);
	boolret &= VtBase::VmCsWrite(GUEST_SS_AR_BYTES, 0xC093);
	boolret &= VtBase::VmCsWrite(GUEST_SS_BASE, 0);

	VtBase::VmCsWrite(GUEST_CR3, current_process->DirectoryTableBase);

	return boolret;
}

// ģ�� SysRet ����
bool VtSsdtHook::VtSysRetEmulate(ULONG_PTR * Registers)
{
	// ��ȡ������Ϣ
	//PNT_KPROCESS current_process = (PNT_KPROCESS)PsGetCurrentProcess();
	ULONG_PTR MsrValue = 0;
	BOOLEAN boolret = TRUE;
	ULONG_PTR GuestRflags = 
		(Registers[R_R11] & ~(X86_FLAGS_RF | X86_FLAGS_VM | X86_FLAGS_RESERVED_BITS)) | X86_FLAGS_FIXED;
	ULONG_PTR exitInstructionLength = VtBase::VmCsRead(VM_EXIT_INSTRUCTION_LEN); // �˳���ָ���

	UNREFERENCED_PARAMETER(exitInstructionLength);

	// �ο���Ƥ�� SYSRET��Return From Fast System Call
	/*
		a. It does so by loading RIP from RCX and loading RFLAGS from R11
		b. ���� RCX ֵ���ص� RIP , �� R11 ��ֵ���ص� RFLAGS ��������һ��
	*/
	boolret &= VtBase::VmCsWrite(GUEST_RIP, Registers[R_RCX]);
	boolret &= VtBase::VmCsWrite(GUEST_RFLAGS, GuestRflags);

	/*
		a. SYSRET loads the CS and SS selectors with values derived from bits 63:48 of the IA32_STAR MSR.
		b. SysRet ���� CS��SS �μĴ�����ֵ������ IA32_STAR MSR �Ĵ����� 48:63 λ
	*/
	MsrValue = __readmsr(MSR_IA32_STAR);
	ULONG_PTR Cs = (UINT16)(((MsrValue >> 48) + 16) | 3);
	boolret &= VtBase::VmCsWrite(GUEST_CS_SELECTOR, Cs);
	boolret &= VtBase::VmCsWrite(GUEST_CS_LIMIT, (UINT32)~0);
	boolret &= VtBase::VmCsWrite(GUEST_CS_AR_BYTES, 0xA0FB);
	boolret &= VtBase::VmCsWrite(GUEST_CS_BASE, 0);

	ULONG_PTR Ss = (UINT16)(((MsrValue >> 48) + 8) | 3);
	boolret &= VtBase::VmCsWrite(GUEST_SS_SELECTOR, Ss);
	boolret &= VtBase::VmCsWrite(GUEST_SS_LIMIT, (UINT32)~0);
	boolret &= VtBase::VmCsWrite(GUEST_SS_AR_BYTES, 0xC0F3);
	boolret &= VtBase::VmCsWrite(GUEST_SS_BASE, 0);

	return boolret;
}

// ����SsdtHook
bool VtSsdtHook::VtStartHookSsdt()
{
	if (GetWindowsVersion() == WIN7) {
		VtSsdtHook::VtHookMsrLstar();	// Msr Hook
	}
	else if (GetWindowsVersion() == WIN10) {
		Asm_VmxCall(CallSsdtHook);		// Efer Hook
	}
	else {
		return false;
	}

	return true;
}

// ֹͣSsdtHook
bool VtSsdtHook::VtStopHookSsdt()
{
	if (GetWindowsVersion() == WIN7) {
		// Win7 �޸� Msr
		VtSsdtHook::VtUnHookMsrLstar();
	}
	else if (GetWindowsVersion() == WIN10) {
		Ia32VmxEfer ia32_efer = { 0 };
		ia32_efer.all = __readmsr(MSR_IA32_EFER);
		ia32_efer.Bits.sce = true;
		// sce λ��Ϊ1
		VtBase::VmCsWrite(GUEST_EFER, ia32_efer.all);
	}
	else {
		return false;
	}

	return true;
}
