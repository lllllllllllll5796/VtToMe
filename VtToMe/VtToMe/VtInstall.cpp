#include "VtInstall.h"
#include "NeedKernelFunc.h"
#include "VtBase.h"
#include "VtEpt.h"
#include "VtSsdtHook.h"
#include "Tools.h"

VtBase * g_Vmxs[128] = { 0 }; // ���֧�� 128 ��
VtEpt * g_Ept;		// VtEpt���ָ��
VtInformaitonEntry g_vt_informaiton = { 0 };

VtInstall::VtInstall()
{
}

VtInstall::~VtInstall()
{
}

// ���� VT �Ķ����Ⱦ DPC �ص�
VOID VtLoadProc(
	_In_ struct _KDPC* Dpc,
	_In_opt_ PVOID DeferredContext,
	_In_opt_ PVOID SystemArgument1,
	_In_opt_ PVOID SystemArgument2
)
{
	UNREFERENCED_PARAMETER(Dpc);
	UNREFERENCED_PARAMETER(DeferredContext);

	BOOLEAN is_user_ept = (g_vt_informaiton.u1.Bits.user_ept ? TRUE : FALSE);
	ULONG uCpuNumber = KeGetCurrentProcessorNumber();
	
	// ���� VT
	g_Vmxs[uCpuNumber] = new VtBase();
	g_Vmxs[uCpuNumber]->m_CpuNumber = uCpuNumber;
	g_Vmxs[uCpuNumber]->VtIsUseEpt = is_user_ept; // �Ƿ�ʹ�� EPT
	
	if (!g_Vmxs[uCpuNumber]->VtCheckIsEnable() && 
		g_Vmxs[uCpuNumber]->VtCheckIsSupported())
	{
		// ����ʹ�� VT ���⻯������
		g_Vmxs[uCpuNumber]->VtEnable(); // ���� VT
		if (g_vt_informaiton.u1.Bits.user_ssdt_hook) { // �Ƿ����� SSDT HOOK
			VtSsdtHook::VtStartHookSsdt();
		}
	}
	else
	{
		kprint(("[+]Cpu:[%d] ���� VT ʧ��\n", uCpuNumber));
	}

	KeSignalCallDpcSynchronize(SystemArgument2);
	KeSignalCallDpcDone(SystemArgument1);
}

// ���� VT �Ķ����Ⱦ DPC �ص�
VOID VtUnLoadProc(
	_In_ struct _KDPC* Dpc,
	_In_opt_ PVOID DeferredContext,
	_In_opt_ PVOID SystemArgument1,
	_In_opt_ PVOID SystemArgument2
)
{
	UNREFERENCED_PARAMETER(Dpc);
	UNREFERENCED_PARAMETER(DeferredContext);
	
	ULONG uCpuNumber = KeGetCurrentProcessorNumber();
	if (g_Vmxs[uCpuNumber]->VtCheckIsSupported())
	{
		g_Vmxs[uCpuNumber]->VtClose(); // �ر� VT
		delete g_Vmxs[uCpuNumber];
		g_Vmxs[uCpuNumber] = NULL;
		// ֪ͨ
		*(PBOOLEAN)DeferredContext = FALSE;

		if (g_vt_informaiton.u1.Bits.user_ssdt_hook)
		{
			VtSsdtHook::VtStopHookSsdt();
		}
	}
	else
	{
		kprint(("[+]Cpu:[%d] �ر� VT ʧ��\n", uCpuNumber));
	}

	KeSignalCallDpcSynchronize(SystemArgument2);
	KeSignalCallDpcDone(SystemArgument1);
}

// �����Ⱦ����
// @is_user_ept �Ƿ�ʹ�� ept
VOID VtInstall::VtStartCpusDrawing()
{

	BOOLEAN is_user_ept_confirmation = g_vt_informaiton.u1.Bits.user_ept;
	if (is_user_ept_confirmation && !g_Ept)
	{
		// ����һ�ξͿ�����(ȫ�� EPT ģʽ)
		g_Ept = new VtEpt();
		if (!g_Ept->VtStartEpt()) // ���� EPT
		{
			// ������� EPT ʧ��
			is_user_ept_confirmation = FALSE; // ֹͣʹ�� EPT
			delete g_Ept;
			g_Ept = NULL;
		}
	}

	g_vt_informaiton.u1.Bits.user_ept = is_user_ept_confirmation;
	KeGenericCallDpc(VtLoadProc, NULL);
}

// �����Ⱦ����
VOID VtInstall::VtEndCpusDrawing()
{
	BOOLEAN g_timeout = TRUE;
	
	KeGenericCallDpc(VtUnLoadProc, (PVOID)&g_timeout);

	// �ȴ� VT ����
	while (true) {
		if (!g_timeout) {
			break;
		}
		if (g_timeout > 30 * 1000 * 1000) {
			return VOID();
		}
	}
	
	if (g_Ept && MmIsAddressValid(g_Ept))
	{
		// ж�� EPT
		g_Ept->VtCloseEpt();
		delete g_Ept;
		g_Ept = NULL;
	}
}

// �򻯰����� VT ���ҳ�ʼ��һЩ��
BOOLEAN VtInstall::VtSimplifyStart(VtInformaitonEntry vt_information)
{
	BOOLEAN result = TRUE;
	NTSTATUS status = STATUS_SUCCESS;
	
	// ��ʼ�� Tools ��
	status = VtInitTools((PDRIVER_OBJECT)vt_information.driver_object);
	if (!NT_SUCCESS(status)) {
		kprint(("VtInitTools ʧ��!\r\n"));
		return FALSE;
	}
	// ��ʼ�� SSDT Hook ��
	result = VtSsdtHook::VtInitSsdtHook();
	if (!result) {
		kprint(("VtInitSsdtHook ʧ��!\r\n"));
		return result;
	}

	// ��ȡ VT ��ʼ����Ϣ
	g_vt_informaiton = vt_information;
	VtStartCpusDrawing();
	return result;
}

// �򻯰���� VT
BOOLEAN VtInstall::VtSimplifyStop()
{
	VtEndCpusDrawing();
	return TRUE;
}
