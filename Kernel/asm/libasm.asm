GLOBAL cpuVendor
GLOBAL getKeyboardBuffer

GLOBAL getSecond
GLOBAL getMinute
GLOBAL getHour

GLOBAL setPITMode
GLOBAL setPITFrequency
GLOBAL setSpeaker

GLOBAL getRegisterSnapshot

GLOBAL stackInit
GLOBAL processExit
GLOBAL semLock
GLOBAL semUnlock

EXTERN register_snapshot
EXTERN register_snapshot_taken
EXTERN kill
EXTERN getCurrentProcess



section .text

getKeyboardBuffer:
	push rbp
	mov rbp, rsp

	in al, 60h

	mov rsp, rbp
	pop rbp

	ret


getSecond:
	push rbp
	mov rbp, rsp

	mov al, 0
	out 70h, al
	in al, 71h

	mov rsp, rbp
	pop rbp

	ret


getMinute:
	push rbp
	mov rbp, rsp

	mov al, 2
	out 70h, al
	in al, 71h

	mov rsp, rbp
	pop rbp

	ret


getHour:
	push rbp
	mov rbp, rsp

	mov al, 4
	out 70h, al
	in al, 71h

	mov rsp, rbp
	pop rbp

	ret


setPITMode:
	push rbp
	mov rbp, rsp
	
	mov rax, rdi
	out 0x43, al

	mov rsp, rbp
	pop rbp

	ret


setPITFrequency:
	push rbp
	mov rbp, rsp

	mov rax, rdi
	out 0x42, al
	mov al, ah
	out 0x42, al

	mov rsp, rbp
	pop rbp
	ret


setSpeaker:
	push rbp
	mov rbp, rsp

	cmp rdi, 0
	je .off

	.on:
	in al, 0x61
	or al, 0x03
	out 0x61, al
	jmp .end

	.off:
	in al, 0x61
	and al, 0xFC
	out 0x61, al

	.end:
	mov rsp, rbp
	pop rbp
	ret

stackInit:

	; Calling convention:
	; rdi -> stack_top (rsp)
	; rsi -> function (rip)
	; rdx -> argc
	; rcx -> argv

	push rbp
	mov rbp, rsp
	
	; Save rdi and rsi in registers that we won't clobber
	; Use r8 and r9 (they're preserved by calling convention, but we're not calling anything)
	mov r8, rdi  ; Save stack_top in r8
	mov r9, rsi  ; Save function/rip in r9
	; rdx (argc) and rcx (argv) are preserved by calling convention
	
	mov rsp, r8	; Switch to process stack top
	
	; Push return address for when main function returns
	; This ensures that when the function does 'ret', it goes to processExit
	mov r10, processExit
	push r10
	
	; Save the adjusted RSP (after pushing return address)
	; This will be the RSP when the function starts executing
	mov r11, rsp
	
	; Build interrupt frame (bottom of stack, will be popped last by iretq)
	push 0x0 ; SS
	push r11 ; RSP (stack with return address already pushed)
	push 0x202 ; RFLAGS (interrupts enabled)
	push 0x8  ; CS (kernel code segment)
	push r9 ; RIP (function address)
	
	; Push all general purpose registers (as if returning from interrupt)
	; Order must match popState macro: R15, R14, ..., RAX
	push 0   ; rax
	push 0   ; rbx
	push 0   ; rcx  
	push 0   ; rdx
	push 0   ; rbp
	push rdx ; rdi (argc - still in rdx from original call)
	push rcx ; rsi (argv - still in rcx from original call)
	push 0   ; r8
	push 0   ; r9
	push 0   ; r10
	push 0   ; r11
	push 0   ; r12
	push 0   ; r13
	push 0   ; r14
	push 0   ; r15

	; Return the current RSP (points to top of saved registers, R15)
	mov rax, rsp
	
	; Restore original stack
	mov rsp, rbp
	pop rbp  ; Restore original rbp
	
	ret

; processExit: Called when a process function returns
; This wrapper terminates the process and triggers a scheduler interrupt
processExit:
	; Get current process PID and kill it
	call getCurrentProcess
	test rax, rax
	jz .hang              ; If no current process, hang
	
	mov rdi, [rax]        ; Get PID (first field in Process struct)
	call kill             ; Call kill(current_pid)
	
	; Force a scheduler interrupt by calling yield
	int 0x20              ; Timer interrupt to force scheduler
	
	.hang:
	hlt
	jmp .hang


; spinlock semaphore implementation based on wiki osdev
semLock:
    mov al, 1
    xchg al, BYTE [rdi]
    cmp al, 0
    jne semLock
    ret

semUnlock:
    mov BYTE [rdi], 0
    ret
