#include "VtHookVector.h"
#include "VtBase.h"


VtHookVector::VtHookVector()
{
}

VtHookVector::~VtHookVector()
{
}

// ��� Idt Hook (�޸�IDT��Ӧ��OFFSET)
void VtHookVector::VtAddHookIdtVector(int vector, void* funcAddress)
{
	ULONG_PTR gdtl = VtBase::VmCsRead(GUEST_GDTR_LIMIT);
	if ((vector > gdtl) && (!funcAddress)) {
		// ���� Gdt �������±� or ��������
		kprint(("��������!\r\n"));
		return void();
	}

	// ��ȡ Idt ��ַ
	ULONG_PTR idt_base = VtBase::VmCsRead(GUEST_IDTR_BASE);
	// ��д����������Ϣ
	pIdtEntry64 idt_entry_x64 = (pIdtEntry64)(idt_base + vector * 16); // ע��x64��һ��idt entry sizeΪ16
	idt_entry_x64->offset_high = HIDWORD(funcAddress);
	idt_entry_x64->idt_entry.Bits.offset_middle = HIWORD(LODWORD(funcAddress));
	idt_entry_x64->idt_entry.Bits.offset_low = LOWORD(LODWORD(funcAddress));

	VtBase::VmCsWrite(GUEST_IDTR_BASE, idt_base);
}
