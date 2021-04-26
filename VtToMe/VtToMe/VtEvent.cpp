#include "VtEvent.h"
#include "VtBase.h"

VtEvent::VtEvent()
{}

VtEvent::~VtEvent()
{}

// @explain: ע���ж�ָ��
// @parameter: InterruptionType interruption_type	�ж�����
// @parameter: InterruptionVector vector	�ж�������		 	
// @parameter: bool deliver_error_code		�Ƿ��д�����
// @parameter: ULONG32 error_code			�еĻ�����д
// @return:  void	�������κ�ֵ
void VtEvent::VtInjectInterruption(
	InterruptionType interruption_type, InterruptionVector vector, 
	bool deliver_error_code, ULONG32 error_code)
{
	VmxVmExit_Interrupt_info inject_event = { 0 };
	inject_event.Bits.valid = true;
	inject_event.Bits.interruption_type = static_cast<ULONG32>(interruption_type);
	inject_event.Bits.vector = static_cast<ULONG32>(vector);
	inject_event.Bits.error_code_valid = deliver_error_code;
	VtBase::VmCsWrite(VmcsField::VM_ENTRY_INTR_INFO, inject_event.all);

	if (deliver_error_code)
	{
		// ����д�����
		VtBase::VmCsWrite(VmcsField::VM_ENTRY_EXCEPTION_ERROR_CODE, error_code);
	}
}

// ͨ������MTFλ, ���� MTF
void VtEvent::VtSetMonitorTrapFlag()
{
	VmxProcessorBasedControls vm_procctl = { 0 };
	vm_procctl.all = static_cast<ULONG32>(VtBase::VmCsRead(CPU_BASED_VM_EXEC_CONTROL));
	vm_procctl.Bits.monitor_trap_flag = true; // ���� MTF
	VtBase::VmCsWrite(CPU_BASED_VM_EXEC_CONTROL, vm_procctl.all);
}

// �ر�MTFλ
void VtEvent::VtCloseMonitorTrapFlag()
{
	VmxProcessorBasedControls vm_procctl = { 0 };
	vm_procctl.all = static_cast<ULONG32>(VtBase::VmCsRead(CPU_BASED_VM_EXEC_CONTROL));
	vm_procctl.Bits.monitor_trap_flag = false; // �ر� MTF
	VtBase::VmCsWrite(CPU_BASED_VM_EXEC_CONTROL, vm_procctl.all);
}
