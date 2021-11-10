
.text
.global ctx_switch
ctx_switch:
	mov %rbx, 0*8(%rdi)
	mov %r12, 1*8(%rdi)
	mov %r13, 2*8(%rdi)
	mov %r14, 3*8(%rdi)
	mov %r15, 4*8(%rdi)
	mov %rsp, 5*8(%rdi)
	mov %rbp, 6*8(%rdi)

	mov 0*8(%rsi), %rbx
	mov 1*8(%rsi), %r12
	mov 2*8(%rsi), %r13
	mov 3*8(%rsi), %r14
	mov 4*8(%rsi), %r15
	mov 5*8(%rsi), %rsp
	mov 6*8(%rsi), %rbp

	ret
