#pragma once
#ifndef VTEPT
#define VTEPT

#include "VtHeader.h"

// ������Ҫ���� EPT �ĳ�ʼ������չ

#pragma pack(push,1) // ע��1�ֽڶ���
/****************************************************EPT��ؽṹ�嶨��******************************************************/

// See: Extended-Page-Table Pointer (EPTP)
// ���� Eptp �ṹ�� (�ο� �����������⻯������(��3.5.17��))
typedef union _EptPointer
{
	ULONG64 all;
	struct
	{
		ULONG64 memory_type : 3;	   //!< [0:2] ָʾ EPT paging-structure ���ڴ�����(����VMM�������������), ��ǰ֧��UC(0)/WB(6)����
		ULONG64 page_walk_length : 3;  //!< [3:5] ָʾ EPT ҳ��ṹ, ���ֵ����1���������ļ�����(�磺����ֵΪ3ʱ��ʾ��4��ҳ��ṹ)
		ULONG64 enable_accessed_and_dirty_flags : 1;  //!< [6] ָʾEPTҳ��ṹ�����access��dirty��־λ��Ч��(�������Ż������������־λ)
		ULONG64 reserved1 : 5;		   //!< [7:11] ָʾ EPT ҳ��ṹ, ���ֵ����1���������ļ�����(�磺����ֵΪ3ʱ��ʾ��4��ҳ��ṹ)
		ULONG64 pml4_address : 36;	   //!< [12:48-1] EPT PML4T PHY ��ַ
		ULONG64 reserved2 : 16;        //!< [48:63]
	}Bits;
}EptPointer, *pEptPointer;
static_assert(sizeof(EptPointer) == sizeof(ULONG64), "EptPointer Size Mismatch");

// See: Format of an EPT PML4 Entry (PML4E) that References an EPT Page-Directory-Pointer Table
//      Page-Directory-Pointer Table
// ���� PML4 �ṹ�� (�ο� ��Ƥ�� Vol. 3C 28-3, �����������⻯������(��6.1.5��))
typedef union _EptPml4Entry
{
	ULONG64 all;
	struct {
		ULONG64 read_access : 1;                                  //!< [0]		�Ƿ�ɶ�
		ULONG64 write_access : 1;                                 //!< [1]		�Ƿ��д
		ULONG64 execute_access : 1;                               //!< [2]		�Ƿ��ִ��
		ULONG64 reserved1 : 5;                                    //!< [3:7]	����
		ULONG64 accessed : 1;                                     //!< [8]		ҳ���Ƿ񱻷���
		ULONG64 ignored1 : 1;                                     //!< [9]
		ULONG64 execute_access_for_user_mode_linear_address : 1;  //!< [10]		Execute access for user-mode linear addresses.
		ULONG64 ignored2 : 1;                                     //!< [11]
		ULONG64 pdpt_address : 36;                                //!< [12:48-1] EPT PDPT �� PHY ��ַ (N == 48)
		ULONG64 reserved2 : 4;                                    //!< [48:51]
		ULONG64 ignored3 : 12;                                    //!< [52:63]
	}Bits;
}EptPml4Entry, *pEptPml4Entry;
static_assert(sizeof(EptPml4Entry) == sizeof(ULONG64), "EptPml4Entry Size Mismatch");

// See: Format of an EPT Page-Directory-Pointer-Table Entry (PDPTE) that References an EPT Page Directory
// ���� PDPTE �ṹ�� (�ο� ��Ƥ�� 28-6 Vol. 3C, �����������⻯������(��6.1.5��))
typedef union _EptPdptEntry
{
	ULONG64 all;
	struct {
		ULONG64 read_access : 1;                                  //!< [0]		�Ƿ�ɶ�
		ULONG64 write_access : 1;                                 //!< [1]		�Ƿ��д
		ULONG64 execute_access : 1;                               //!< [2]		�Ƿ��ִ��
		ULONG64 reserved1 : 5;                                    //!< [3:7]
		ULONG64 accessed : 1;                                     //!< [8]		ҳ���Ƿ񱻷���
		ULONG64 ignored1 : 1;                                     //!< [9]
		ULONG64 execute_access_for_user_mode_linear_address : 1;  //!< [10]
		ULONG64 ignored2 : 1;                                     //!< [11]
		ULONG64 pdt_address : 36;                                 //!< [12:48-1] EPT PDT �� PHY ��ַ (N == 48)
		ULONG64 reserved2 : 4;                                    //!< [48:51]
		ULONG64 ignored3 : 12;                                    //!< [52:63]
	}Bits;
}EptPdptEntry, *pEptPdptEntry;
static_assert(sizeof(EptPdptEntry) == sizeof(ULONG64), "EptPdptEntry Size Mismatch");

// See: Format of an EPT Page-Directory Entry (PDE) that References an EPT Page Table
// ���� PDPTE �ṹ�� (�ο� ��Ƥ�� 28-8 Vol. 3C, �����������⻯������(��6.1.5��))
typedef union _EptPdEntry
{
	ULONG64 all;
	struct {
		ULONG64 read_access : 1;                                  //!< [0]		�Ƿ�ɶ�
		ULONG64 write_access : 1;                                 //!< [1]		�Ƿ��д
		ULONG64 execute_access : 1;                               //!< [2]		�Ƿ��ִ��
		ULONG64 reserved1 : 4;                                    //!< [3:6]
		ULONG64 must_be0 : 1;                                     //!< [7]		Must be 0 (otherwise, this entry maps a 2-MByte page)
		ULONG64 accessed : 1;                                     //!< [8]		�Ƿ񱻷��ʹ�
		ULONG64 ignored1 : 1;                                     //!< [9]		
		ULONG64 execute_access_for_user_mode_linear_address : 1;  //!< [10]
		ULONG64 ignored2 : 1;                                     //!< [11]
		ULONG64 pt_address : 36;                                  //!< [12:48-1] EPT PT �� PHY ��ַ (N == 48)
		ULONG64 reserved2 : 4;                                    //!< [48:51]
		ULONG64 ignored3 : 12;                                    //!< [52:63]
	}Bits;
}EptPdEntry, *pEptPdEntry;
static_assert(sizeof(EptPdEntry) == sizeof(ULONG64), "EptPdEntry Size Mismatch");

// See: Format of an EPT Page-Table Entry that Maps a 4-KByte Page
// ���� PTE �ṹ�� (�ο� ��Ƥ�� 28-8 Vol. 3C 28-9, �����������⻯������(��6.1.5��))
typedef union _EptPtEntry
{
	ULONG64 all;
	struct {
		ULONG64 read_access : 1;                                  //!< [0]		�Ƿ�ɶ�
		ULONG64 write_access : 1;                                 //!< [1]		�Ƿ��д
		ULONG64 execute_access : 1;                               //!< [2]		�Ƿ��ִ��
		ULONG64 memory_type : 3;                                  //!< [3:5]	ָʾ guest-physical address ҳ�� cache ���ڴ�����
		ULONG64 ignore_pat_memory_type : 1;                       //!< [6]		Ϊ1ʱ, PAT�ڴ����ͱ�����(��CR0.CD = 0ʱ, ���յ�HPAҳ����ڴ�������PAT�ڴ����ͼ�EPT�ڴ��������Ͼ���)
		ULONG64 ignored1 : 1;                                     //!< [7]
		ULONG64 accessed : 1;                                     //!< [8]		�Ƿ񱻷��ʹ�
		ULONG64 dirty_written : 1;                                //!< [9]		�Ƿ�д��
		ULONG64 execute_access_for_user_mode_linear_address : 1;  //!< [10]
		ULONG64 ignored2 : 1;                                     //!< [11]
		ULONG64 physial_address : 36;                             //!< [12:48-1] EPT 4kb�ڴ� �� PHY ��ַ (N == 48)
		ULONG64 reserved1 : 4;                                    //!< [48:51]
		ULONG64 Ignored3 : 11;                                    //!< [52:62]
		ULONG64 suppress_ve : 1;                                  //!< [63]
	}Bits;
}EptPtEntry, *pEptPtEntry;
static_assert(sizeof(EptPtEntry) == sizeof(ULONG64), "EptPtEntry Size Mismatch");

/*************************************************************************************************************************/
#pragma pack(pop)

class VtEpt : public VtHeader
{
public:
	VtEpt();
	~VtEpt();

public:
	// Ept ��Ϣ
	EptPointer m_Eptp;

private:
	// ���� EPTP �����Ϣ
	VOID VtSetEptPointer(PVOID Pml4Address);

public:
	// ͨ�������ַ��ȡ Ept PTE ָ��
	pEptPtEntry VtGetPteByPhyAddress(ULONG_PTR PhyAddress);

public:
	// ����	EPT
	BOOLEAN	VtStartEpt();
	// �ر�	EPT
	VOID	VtCloseEpt();

public:
	// ���� Ept VM-exit �����Ļص�
	VOID EptViolationVtExitHandler(ULONG_PTR * Registers);

protected:
	// ��ʼ�� EPT
	BOOLEAN VtInitEpt();
	

};

#endif

