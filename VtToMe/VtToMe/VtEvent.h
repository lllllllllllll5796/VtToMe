#pragma once
#ifndef VTEVENT
#define VTEVENT

#include "VtHeader.h"

// �������ڶ� VM(Guest) ���������¼���ע��

class VtEvent
{
public:
	VtEvent();
	~VtEvent();

public:
	// ע���ж�ָ��
	static void VtInjectInterruption(
		InterruptionType interruption_type, InterruptionVector vector,
		bool deliver_error_code, ULONG32 error_code);

public:
	// ʹ��(pending) MTF �����ַ�ʽ (�ο������������⻯������(��4.14��))
	// (1). ͨ���¼�ע�뷽ʽ, ע��һ���ж�����Ϊ7(other)����������Ϊ0���¼���MTF VM-exit �� VM-entry ��ֱ��pending��guest��һ��ָ��ǰ��
	// (2). ͨ������MTFλ(monitor trap flag)��ʽ��MTF VM-exit �� VM-entry ��ֱ��pending��guest��һ��ָ���������trap���͵� VM-exit ��
	static void VtSetMonitorTrapFlag();
	static void VtCloseMonitorTrapFlag();
};

#endif
