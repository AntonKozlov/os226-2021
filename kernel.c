
#define _GNU_SOURCE

#include "syscall.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/ucontext.h>

extern int shell(int argc, char *argv[]);

static void sighnd(int sig, siginfo_t *info, void *ctx) {
    ucontext_t *uc = (ucontext_t *) ctx;
    greg_t *regs = uc->uc_mcontext.gregs;

    uint16_t insn = *(uint16_t *) regs[REG_RIP];
    if (insn != 0x81cd) {
        abort();
    }

    regs[REG_RAX] = syscall_do(regs[REG_RAX], regs[REG_RBX],
                               regs[REG_RCX], regs[REG_RDX],
                               regs[REG_RSI], (void *) regs[REG_RDI]);

    regs[REG_RIP] += 2;
}

int main(int argc, char *argv[]) {
	struct sigaction act = {
		.sa_sigaction = sighnd,
		.sa_flags = SA_RESTART,
	};
	sigemptyset(&act.sa_mask);

	if (-1 == sigaction(SIGSEGV, &act, NULL)) {
		perror("signal set failed");
		return 1;
	}

	shell(0, NULL);
}
