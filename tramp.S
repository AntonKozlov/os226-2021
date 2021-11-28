
.text
.global tramptramp, exittramp
tramptramp:
	pushfq      /* сохраняем регистры и флаги */
	push %rax
	push %rbx
	push %rcx
	push %rdx
	push %rsi
	push %rdi
	push %r8
	push %r9
	push %r10
	push %r11
	push %r12
	push %r13
	push %r14
	push %r15
	push %rbp   /* rbp is the frame pointer. Тоже сохраняем, тк при вызове call может измениться */

	mov %rsp, %rbx  /* сохраняем rsp */
	mov %rsp, %rdi  /* передаем rsp как первыйй аргумент в bottom */
    mov 18*8(%rbx), %rsi    /* sig как второй аргумент */
    mov 17*8(%rbx), %rsp    /* переходим на стек текущего task */
    call *16*8(%rbx)    /* вызываем bottom */
	mov %rbx, %rsp  /* возвращаемся на наш стек */

exittramp:
    pop %rbp    /* восстанавливаем реистры и флаги */
	pop %r15
	pop %r14
	pop %r13
	pop %r12
	pop %r11
	pop %r10
	pop %r9
	pop %r8
	pop %rdi
	pop %rsi
	pop %rdx
	pop %rcx
	pop %rbx
	pop %rax
	popfq

	mov 3*8(%rsp), %rsp     /* возвращаем rsp сохраненный в top */
	jmp *-(128/*red_zone*/ + 8/*rip push*/)(%rsp)   /* переходим по rip в top, те выходим из top */
