#pragma once
#ifndef VTHOOKVECTOR
#define VTHOOKVECTOR

// ������Ҫ���� Hook Gdt or Idt �ж�

#include "VtHeader.h"

class VtHookVector : VtHeader
{
public:
	VtHookVector();
	~VtHookVector();

public:
	// ��� Idt Hook (�޸�IDT��Ӧ��OFFSET)
	static void VtAddHookIdtVector(int vector, void* funcAddress);
	// ��� Gdt Hook
};

#endif

