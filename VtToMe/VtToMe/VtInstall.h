#pragma once
#ifndef VTINSTALL
#define VTINSTALL

// ������Ҫ���� VT �������͹ر�

#include "VtHeader.h"

class VtInstall : public VtHeader
{
public:
	VtInstall();
	~VtInstall();

public:
	// �����Ⱦ����
	static VOID VtStartCpusDrawing();
	// �����Ⱦ����
	static VOID VtEndCpusDrawing();

	// �򻯰����� VT ���ҳ�ʼ��һЩ��
	static BOOLEAN VtSimplifyStart(VtInformaitonEntry is_use_ept);
	// �򻯰���� VT
	static BOOLEAN VtSimplifyStop();
};

#endif

