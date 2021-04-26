#pragma once
#ifndef VTBASE
#define VTBASE

#include "VtHeader.h"
#include "VtAsm.h"
#include <intrin.h>

// ������Ҫ���ڹ��� VT VMM �������Ϣ

#define SELECTOR_TABLE_INDEX    0x04

class VtBase : public VtHeader
{
public:
	VtBase();
	~VtBase();

public:
	// ��ȡ�˴��� Rip\Rsp, ��Ϊ GUEST �Ļ���
	ULONG_PTR GuestRip, GuestRsp;

public:
	ULONG32 m_CpuNumber; // ��ǰCPU���

	volatile BOOLEAN m_VmxOn; // ��ǰ CPU �˵����⻯�Ƿ��

	ULONG_PTR * m_VmOnRegionAddress;     // VMON ����
	ULONG_PTR * m_VmCsRegionAddress;     // VMCS ����
	ULONG_PTR * m_VmBitMapRegionAddress; // VM BITMAP ����

	ULONGLONG m_VmOnRegionPhyAddress;    // ��Ӧ�������ַ
	ULONGLONG m_VmCsRegionPhyAddress;
	ULONGLONG m_VmMsrBitMapRegionPhyAddress;

	ULONG_PTR m_VmStackRootRegionPointer;// VMM ����Ҫ�Ķ�ջ�ڴ�

	HOST_STATE  m_HostState;  // HOST  ����
	GUEST_STATE m_GuestState; // GUEST ����

	BOOLEAN VtIsUseEpt; // �Ƿ�ʹ�� EPT

public:
	// VMCS ����Ķ�д
	static BOOLEAN VmCsWrite(ULONG_PTR info, ULONG_PTR Value);
	static ULONG_PTR VmCsRead(ULONG_PTR info);

	// ��������ָ��Bits
	ULONG VmxAdjustControlValue(ULONG Msr, ULONG Ctl)
	{
		// �ο��ԡ����������⻯������(2.5.6.3)
		LARGE_INTEGER MsrValue = { 0 };
		MsrValue.QuadPart = __readmsr(Msr);
		Ctl &= MsrValue.HighPart;     //ǰ32λΪ0��λ�ñ�ʾ��Щ��������λ0
		Ctl |= MsrValue.LowPart;      //��32λΪ1��λ�ñ�ʾ��Щ��������λ1
		return Ctl;
	}

	// ��ȡ��Ӧ�ĶμĴ�����Ϣ
	static VOID GetSelectorInfoBySelector(ULONG_PTR selector, ULONG_PTR * base, ULONG_PTR * limit, ULONG_PTR * attribute)
	{
		GDT gdtr;
		PKGDTENTRY64 gdtEntry;

		//��ʼ��Ϊ0
		*base = *limit = *attribute = 0;

		if (selector == 0 || (selector & SELECTOR_TABLE_INDEX) != 0) {
			*attribute = 0x10000;	// unusable
			return;
		}

		__sgdt(&gdtr);
		gdtEntry = (PKGDTENTRY64)(gdtr.uBase + (selector & ~(0x3)));

		*limit = __segmentlimit((ULONG32)selector);
		*base = ((gdtEntry->u1.Bytes.BaseHigh << 24) | (gdtEntry->u1.Bytes.BaseMiddle << 16) | (gdtEntry->u1.BaseLow)) & 0xFFFFFFFF;
		*base |= ((gdtEntry->u1.Bits.Type & 0x10) == 0) ? ((uintptr_t)gdtEntry->u1.BaseUpper << 32) : 0;
		*attribute = (gdtEntry->u1.Bytes.Flags1) | (gdtEntry->u1.Bytes.Flags2 << 8);
		*attribute |= (gdtEntry->u1.Bits.Present) ? 0 : 0x10000;

		return VOID();
	}

public:
	// ִ�� VMON ָ��
	BOOLEAN ExecuteVmxOn();

public:
	// ����Ƿ������� VT
	BOOLEAN VtCheckIsSupported();
	// ������⻯�����Ƿ��
	BOOLEAN VtCheckIsEnable();

	// ���� VMON\VMCS �ڴ��ַ
	BOOLEAN VtVmmMemAllocate();
	// �ͷ����� VMM �ڴ�
	VOID VtVmmMemFree();

	// ���� VMCS ����
	BOOLEAN SetupVmcs();
	// ������ VMCS MSR ����
	BOOLEAN InitVmcs();
public:
	// ���� VT
	BOOLEAN VtEnable();

	// �ر� VT
	BOOLEAN VtClose();
};


#endif
