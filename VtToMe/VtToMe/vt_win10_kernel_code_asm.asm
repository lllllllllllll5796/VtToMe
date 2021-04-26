
USERMD_STACK_GS = 10h
KERNEL_STACK_GS = 1A8h
KERNEL_CR3 = 9000h

MAX_SYSCALL_INDEX = 1000h

EXTERN KiSystemCall64Pointer:DQ
EXTERN KiSystemServiceCopyEndPointer:DQ

EXTERN g_hook_enabled:DB
EXTERN g_param_table:DB
EXTERN g_ssdt_table:DQ

.DATA
ServiceNum dq ? ; ϵͳ�����

.CODE

; *********************************************************
;
;	��дWin10ϵͳ��������, ��Ҫע�����΢���"����"����
;
; *********************************************************
Win10_SysCallEntryPointer PROC
	;cli
	
	;mov			gs:[USERMD_STACK_GS], rsp
	;mov			rsp, gs:[KERNEL_CR3]		; �л�Ϊ�ں�Cr3
	;mov			cr3, rsp

	swapgs
	mov			gs:[USERMD_STACK_GS], rsp

	;
	;	�ж�ϵͳ������Ƿ�Ϊ Ӱ�ӱ�(shadow)����
	;
	cmp			rax, MAX_SYSCALL_INDEX		; �Ƿ�ΪӰ�ӱ�(shadow)����
	jge			Win10_KiSystemCall64		; �ǣ�����ԭ��������

	lea			rsp, offset g_hook_enabled	;
	cmp			byte ptr [rsp + rax], 0h	; �Ƿ�ΪHook�ĺ���
	jne			Win10_KiSystemCall64_Emulate		; �ǵĻ�, �������ǵ� syscall ����
Win10_SysCallEntryPointer ENDP

; *********************************************************
;
;	ԭ��������
;
; *********************************************************
Win10_KiSystemCall64 PROC
	mov rsp, gs:[USERMD_STACK_GS]	; ��ԭ����
	swapgs							; Switch to usermode GS
	jmp [KiSystemCall64Pointer]		; ����ԭ��������
Win10_KiSystemCall64 ENDP

; *********************************************************
;
;	���ǵ�SysCall��������
;
; *********************************************************
Win10_KiSystemCall64_Emulate PROC
	mov			rsp, gs:[KERNEL_STACK_GS]   ; set kernel stack pointer
	push		2Bh                         ; push dummy SS selector
    push		qword ptr gs:[10h]          ; push user stack pointer
    push		r11                         ; push previous EFLAGS
    push		33h                         ; push dummy 64-bit CS selector
    push		rcx                         ; push return address
    mov			rcx, r10                    ; set first argument value
	
	sub			rsp, 8h						; 
	push        rbp                         ; save standard register
    sub         rsp, 158h                   ; allocate fixed frame
    lea         rbp, [rsp+80h]              ; set frame pointer
    mov         [rbp+0C0h], rbx             ; save nonvolatile registers
    mov         [rbp+0C8h], rdi             ;
    mov         [rbp+0D0h], rsi				;

Win10_KiSystemServiceUser:
	mov			byte ptr [rbp-55h], 2
	mov			rbx, gs:[188h]
	prefetchw	byte ptr [rbx+90h]				    
	stmxcsr		dword ptr [rbp-54h]				    
	ldmxcsr		dword ptr gs:[180h]				    
	cmp			byte ptr [rbx+3], 0				    
	mov			word ptr [rbp+80h], 0
	jz			Win10_ClearDebugPart		; if CurrentThread.Header.DebugActive is 0, jmp
; -----------------------------------------------------------
	; GG ������ʱ��ʵ���е�������µ���ش���
	INT 3
	align       10h

Win10_ClearDebugPart:
	;sti
	mov			[rbx+88h], rcx
	mov			[rbx+80h], eax
	xchg		ax, ax

Win10_KiSystemServiceStart:
	mov     [rbx+90h], rsp					; CurrentThread.TrapFrame = Rsp(��ǰ_KTRAP_FRAME �ṹ)
	mov     edi, eax						; Rdi = ϵͳ�����
	shr     edi, 7							; Rdi = ϵͳ����� >> 7
	and     edi, 20h						; Rdi = (ϵͳ����� >> 7) & 0x20 = ����������
	and     eax, 0FFFh						; EAX = ϵͳ�����

Win10_KiSystemServiceRepeat:
	; RAX = [IN ] syscall index
    ; RAX = [OUT] number of parameters
    ; R10 = [OUT] func address
    ; R11 = [I/O] trashed

	lea		r11, offset g_ssdt_table			; �л� ssdt ��
	mov		r10, qword ptr [r11 + rax * 8h]		; ��ȡHook������ַ

	lea		r11, offset g_param_table			;
	movzx	rax, byte ptr [r11 + rax]			; RAX = paramter count

	jmp		[KiSystemServiceCopyEndPointer]		; ����ԭ�������� (���� Hook ����������ϵĺ���)
Win10_KiSystemCall64_Emulate ENDP

END