USERMD_STACK_GS = 10h
KERNEL_STACK_GS = 1A8h

MAX_SYSCALL_INDEX = 1000h

EXTERN g_ssdt_table:DQ
EXTERN g_param_table:DB
EXTERN g_hook_enabled:DB

EXTERN KiSystemCall64Pointer:DQ
EXTERN KiSystemServiceCopyEndPointer:DQ

.CODE

; ************************��д Windows7 syscall**************************
Win7_SysCallEntryPointer PROC
	swapgs									; �û� GS ��TEB64��Ϊ KPCR
	mov			gs:[USERMD_STACK_GS], rsp	; save user stack pointer

	cmp			rax, MAX_SYSCALL_INDEX		; �Ƿ�ΪӰ�ӱ�(shadow)����
	jge			KiSystemCall64				; �ǣ�����ԭ��������

	lea			rsp, offset g_hook_enabled	; 
	cmp			byte ptr [rsp + rax], 0h	; �Ƿ�ΪHook�ĺ���
	jne			KiSystemCall64_Emulate		; �������ǵ� syscall ����
Win7_SysCallEntryPointer ENDP
; **************************************************

; ************************��д Windows7 syscall**************************
KiSystemCall64 PROC
	mov rsp, gs:[USERMD_STACK_GS]	; ��ԭ����
	swapgs							; Switch to usermode GS
	jmp [KiSystemCall64Pointer]		; ����ԭ��������
KiSystemCall64 ENDP
; **************************************************

; ************************��д Windows7 syscall**************************
KiSystemCall64_Emulate PROC
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
    mov         [rbp+0D0h], rsi             ;
    mov         byte ptr [rbp-55h], 2h      ; set service active
    mov         rbx, gs:[188h]              ; get current thread address
    prefetchw	byte ptr [rbx+1D8h]         ; prefetch with write intent
    stmxcsr     dword ptr [rbp-54h]         ; save current MXCSR
    ldmxcsr     dword ptr gs:[180h]         ; set default MXCSR
    cmp         byte ptr [rbx+3], 0         ; test if debug enabled
    mov         word ptr [rbp+80h], 0       ; assume debug not enabled
	jz			KiSS50						; if z, debug not enabled
	mov         [rbp-50h], rax              ; save service argument registers
    mov         [rbp-48h], rcx              ;
    mov         [rbp-40h], rdx              ;
    mov         [rbp-38h], r8               ;
    mov         [rbp-30h], r9               ;

	int			3							; ���� INT3 �� KiSaveDebugRegisterState(˼·�е�ɧ)
	align       10h							; ע��16�ֽڶ���

KiSS50:
	mov		[rbx+1E0h], rcx					; CurrentThread.FirstArgument = ������һ������
	mov		[rbx+1F8h], eax					;
KiSystemCall64_Emulate ENDP
; **************************************************

; ************************��д Windows7 syscall**************************
KiSystemServiceStart PROC
	mov     [rbx+1D8h], rsp					; CurrentThread.TrapFrame = Rsp(��ǰ_KTRAP_FRAME �ṹ)
	mov     edi, eax						; Rdi = ϵͳ�����
	shr     edi, 7							; Rdi = ϵͳ����� >> 7
	and     edi, 20h						; Rdi = (ϵͳ����� >> 7) & 0x20 = ����������
	and     eax, 0FFFh						; EAX = ϵͳ�����
KiSystemServiceStart ENDP
; **************************************************

; ************************��д Windows7 syscall**************************
KiSystemServiceRepeat PROC
	; RAX = [IN ] syscall index
    ; RAX = [OUT] number of parameters
    ; R10 = [OUT] func address
    ; R11 = [I/O] trashed

	lea		r11, offset g_ssdt_table			; �л� ssdt ��
	mov		r10, qword ptr [r11 + rax * 8h]		; ��ȡHook������ַ

	lea		r11, offset g_param_table			;
	movzx	rax, byte ptr [r11 + rax]			; RAX = paramter count

	jmp		[KiSystemServiceCopyEndPointer]		; ����ԭ�������� (���� Hook ����������ϵĺ���)
KiSystemServiceRepeat ENDP
; **************************************************

END