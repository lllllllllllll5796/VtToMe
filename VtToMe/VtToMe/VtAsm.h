#pragma once
#ifndef VTASM
#define VTASM

// ���������ṩ VT ����Ҫ�Ļ�ຯ��

#include "VtHeader.h"

// ��Ҫ��ָ��
EXTERN_C ULONG_PTR __readcs(void);
EXTERN_C ULONG_PTR __readds(void);
EXTERN_C ULONG_PTR __readss(void);
EXTERN_C ULONG_PTR __reades(void);
EXTERN_C ULONG_PTR __readfs(void);
EXTERN_C ULONG_PTR __readgs(void);
EXTERN_C ULONG_PTR __sldt(void);
EXTERN_C ULONG_PTR __str(void);
EXTERN_C ULONG_PTR __sgdt(PGDT gdtr);
EXTERN_C void __invd(void);
EXTERN_C void __writeds(ULONG_PTR ds);
EXTERN_C void __writees(ULONG_PTR es);
EXTERN_C void __writefs(ULONG_PTR fs);
EXTERN_C void __writecr2(ULONG_PTR cr2);

// EPT ���ָ��
EXTERN_C void __invept(ULONG_PTR Type, ULONG_PTR * EptpPhyAddr); // ˢ�� EPT
EXTERN_C void __invvpid(ULONG_PTR Type, ULONG_PTR * EptpPhyAddr);

// Host��Guest ����ָ��
EXTERN_C void __GetStackPointer(ULONG_PTR* StackPointer);
EXTERN_C void __GetNextInstructionPointer(ULONG_PTR* RipPointer);

// VmExit ����ָ��
EXTERN_C void Asm_VmExitHandler();

// Vmcall ��ص�ָ��
EXTERN_C void Asm_UpdateRspAndRip(ULONG_PTR Rsp, ULONG_PTR Rip); // �޸ĵ�ǰ Rsp\Rip

// @explain: vmcall ָ��ĵ���
// @parameter: ULONG64 uCallNumber		���
// @parameter: PVOID hook_liner_address	Ҫhook�����Ե�ַ (ע��hook rin3ʱ, Ҫ�Ƚ�cr3���л�Ϊr3�����cr3)
// @parameter: PVOID jmp_liner_address	Ҫ��������Ե�ַ
// @return:  EXTERN_C void
EXTERN_C void Asm_VmxCall(ULONG64 uCallNumber, PVOID hook_liner_address = NULL, PVOID jmp_liner_address = NULL, PVOID * ret_address = NULL);

#endif