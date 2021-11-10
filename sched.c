#define _GNU_SOURCE

#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <elf.h>
#include <sys/mman.h>
#include <sys/fcntl.h>

#include "sched.h"
#include "timer.h"
#include "pool.h"
#include "ctx.h"
#include "syscall.h"

/* AMD64 Sys V ABI, 3.2.2 The Stack Frame:
   The 128-byte area beyond the location pointed to by %rsp is considered to
   be reserved and shall not be modified by signal or interrupt handlers */
#define SYSV_REDST_SZ 128

#define TICK_PERIOD 100

#define MEM_PAGES 1024
#define PAGE_SIZE 4096

#define USER_PAGES 1024
#define USER_START ((void*)IUSERSPACE_START)
#define USER_STACK_PAGES 2

extern int shell(int argc, char *argv[]);

extern void tramptramp(void);

struct vmctx {
	unsigned map[USER_PAGES];
	unsigned brk;
};

struct task {
	char stack[8192];
	struct vmctx vm;

	union {
		struct ctx ctx;
		struct {
			int(*main)(int, char**);
			int argc;
			char **argv;
		};
	};

	void (*entry)(void *as);
	void *as;
	int priority;

	// timeout support
	int waketime;

	// policy support
	struct task *next;
};

static int time;

static int current_start;
static struct task *current;
static struct task *idle;
static struct task *runq;
static struct task *waitq;

static struct task *pendingq;
static struct task *lastpending;

static int (*policy_cmp)(struct task *t1, struct task *t2);

static struct task taskarray[16];
static struct pool taskpool = POOL_INITIALIZER_ARRAY(taskarray);

static sigset_t irqs;

static int memfd = -1;
#define LONG_BITS (sizeof(unsigned long) * CHAR_BIT)
static unsigned long bitmap_pages[MEM_PAGES / LONG_BITS];

void irq_disable(void) {
	sigprocmask(SIG_BLOCK, &irqs, NULL);
}

void irq_enable(void) {
	sigprocmask(SIG_UNBLOCK, &irqs, NULL);
}

static int bitmap_alloc(unsigned long *bitmap, size_t size) {
	unsigned n = size / sizeof(*bitmap);
	unsigned long *w = NULL;
	for (int i = 0; i < n; ++i) {
		if (bitmap[i] != -1) {
			w = &bitmap[i];
			break;
		}
	}
	if (!w) {
		fprintf(stderr, "cannot find free page\n");
		abort();
		return -1;
	}
	int v = ffsl(*w + 1) - 1;
	*w |= 1 << v;
	return v + (w - bitmap) * LONG_BITS;
}

static void bitmap_free(unsigned long *bitmap, size_t size, unsigned v) {
	bitmap[v / LONG_BITS] &= ~(1 << (v % LONG_BITS));
}

static void policy_run(struct task *t) {
	struct task **c = &runq;

	while (*c && (t == idle || policy_cmp(*c, t) <= 0)) {
		c = &(*c)->next;
	}
	t->next = *c;
	*c = t;
}

static void vmctx_make(struct vmctx *vm, size_t stack_size) {
	memset(vm->map, -1, sizeof(vm->map));
	for (int i = 0; i < stack_size / PAGE_SIZE; ++i) {
		int mempage = bitmap_alloc(bitmap_pages, sizeof(bitmap_pages));
		if (mempage == -1) {
			abort();
		}
		vm->map[USER_PAGES - 1 - i] = mempage;
	}
}

static void vmctx_apply(struct vmctx *vm) {
	munmap(USER_START, USER_STACK_PAGES * PAGE_SIZE);
	for (int i = 0; i < USER_PAGES; ++i) {
		if (vm->map[i] == -1) {
			continue;
		}
		void *addr = mmap(USER_START + i * PAGE_SIZE,
				PAGE_SIZE,
				PROT_READ | PROT_WRITE | PROT_EXEC,
				MAP_SHARED | MAP_FIXED,
				memfd, vm->map[i] * PAGE_SIZE);
		if (addr == MAP_FAILED) {
			perror("mmap");
			abort();
		}

		if (addr != USER_START + i * PAGE_SIZE) {
			abort();
		}
	}
}

static void doswitch(void) {
	struct task *old = current;
	current = runq;
	runq = current->next;

	current_start = sched_gettime();
	vmctx_apply(&current->vm);
	ctx_switch(&old->ctx, &current->ctx);
}

static void tasktramp(void) {
	irq_enable();
	current->entry(current->as);
	irq_disable();
	doswitch();
}

static void tasktramp0(void) {
	struct ctx dummy, new;
	vmctx_apply(&current->vm);
	ctx_make(&new, tasktramp, USER_START + USER_PAGES * PAGE_SIZE);
	ctx_switch(&dummy, &new);
}

void sched_new(void (*entrypoint)(void *aspace),
		void *aspace,
		int priority) {

	struct task *t = pool_alloc(&taskpool);
	t->entry = entrypoint;
	t->as = aspace;
	t->priority = priority;
	t->next = NULL;

	vmctx_make(&t->vm, 4 * PAGE_SIZE);
	ctx_make(&t->ctx, tasktramp0, t->stack + sizeof(t->stack));

	if (!lastpending) {
		lastpending = t;
		pendingq = t;
	} else {
		lastpending->next = t;
		lastpending = t;
	}
}

void sched_sleep(unsigned ms) {

	if (!ms) {
		irq_disable();
		policy_run(current);
		doswitch();
		irq_enable();
		return;
	}

	current->waketime = sched_gettime() + ms;

	int curtime;
	while ((curtime = sched_gettime()) < current->waketime) {
		irq_disable();
		struct task **c = &waitq;
		while (*c && (*c)->waketime < current->waketime) {
			c = &(*c)->next;
		}
		current->next = *c;
		*c = current;

		doswitch();
		irq_enable();
	}
}

static int fifo_cmp(struct task *t1, struct task *t2) {
	return -1;
}

static int prio_cmp(struct task *t1, struct task *t2) {
	return t2->priority - t1->priority;
}

static void hctx_push(greg_t *regs, unsigned long val) {
	regs[REG_RSP] -= sizeof(unsigned long);
	*(unsigned long *) regs[REG_RSP] = val;
}

static void bottom(void) {
	time += TICK_PERIOD;

	while (waitq && waitq->waketime <= sched_gettime()) {
		struct task *t = waitq;
		waitq = waitq->next;
		policy_run(t);
	}

	if (TICK_PERIOD <= sched_gettime() - current_start) {
		irq_disable();
		policy_run(current);
		doswitch();
		irq_enable();
	}
}

static void top(int sig, siginfo_t *info, void *ctx) {
	ucontext_t *uc = (ucontext_t *) ctx;
	greg_t *regs = uc->uc_mcontext.gregs;

	unsigned long oldsp = regs[REG_RSP];
	regs[REG_RSP] -= SYSV_REDST_SZ;
	hctx_push(regs, regs[REG_RIP]);
	hctx_push(regs, oldsp);
	hctx_push(regs, (unsigned long) (current->stack + sizeof(current->stack) - 16));
	hctx_push(regs, (unsigned long) bottom);
	regs[REG_RIP] = (greg_t) tramptramp;
}

long sched_gettime(void) {
	int cnt1 = timer_cnt() / 1000;
	int time1 = time;
	int cnt2 = timer_cnt() / 1000;
	int time2 = time;

	return (cnt1 <= cnt2) ?
		time1 + cnt2 :
		time2 + cnt2;
}

void sched_run(enum policy policy) {
	int (*policies[])(struct task *t1, struct task *t2) = { fifo_cmp, prio_cmp };
	policy_cmp = policies[policy];

	struct task *t = pendingq;
	while (t) {
		struct task *next = t->next;
		policy_run(t);
		t = next;
	}

	sigemptyset(&irqs);
	sigaddset(&irqs, SIGALRM);

	timer_init(TICK_PERIOD, top);

	irq_disable();

	idle = pool_alloc(&taskpool);
	memset(&idle->vm.map, -1, sizeof(idle->vm.map));

	current = idle;

	sigset_t none;
	sigemptyset(&none);

	while (runq || waitq) {
		if (runq) {
			policy_run(current);
			doswitch();
		} else {
			sigsuspend(&none);
		}

	}

	irq_enable();
}

static void sighnd(int sig, siginfo_t *info, void *ctx) {
	ucontext_t *uc = (ucontext_t *) ctx;
	greg_t *regs = uc->uc_mcontext.gregs;

	uint16_t insn = *(uint16_t*)regs[REG_RIP];
	if (insn != 0x81cd) {
		abort();
	}

	regs[REG_RAX] = syscall_do(regs[REG_RAX], regs[REG_RBX],
			regs[REG_RCX], regs[REG_RDX],
			regs[REG_RSI], (void *) regs[REG_RDI]);

	regs[REG_RIP] += 2;
}

static int vmctx_brk(struct vmctx *vm, void *addr) {
	int newbrk = (addr - USER_START + PAGE_SIZE - 1) / PAGE_SIZE;
	if ((newbrk < 0) || (USER_PAGES <= newbrk)) {
		fprintf(stderr, "Out-of-mem\n");
		abort();
	}

	for (unsigned i = vm->brk; i < newbrk; ++i) {
		vm->map[i] = bitmap_alloc(bitmap_pages, sizeof(bitmap_pages));
	}
	for (unsigned i = newbrk; i < vm->brk; ++i) {
		bitmap_free(bitmap_pages, sizeof(bitmap_pages), vm->map[i]);
	}
	vm->brk = newbrk;

	return 0;
}

int vmprotect(void *start, unsigned len, int prot) {
#if 0
	if (mprotect(start, len, prot)) {
		perror("mprotect");
		return -1;
	}
#endif
	return 0;
}

static void exectramp(void) {
	irq_enable();
	current->main(current->argc, current->argv);
	irq_disable();
	doswitch();
}

static int do_exec(const char *path, char *argv[]) {
	int fd = open(path, O_RDONLY);
	if (fd < 0) {
		perror("open");
		return 1;
	}

	void *rawelf = mmap(NULL, 128 * 1024, PROT_READ, MAP_PRIVATE, fd, 0);

	if (strncmp(rawelf, "\x7f" "ELF" "\x2", 5)) {
		printf("ELF header mismatch\n");
		return 1;
	}

	Elf64_Ehdr eh;
	Elf64_Phdr ph;
	read(fd, &eh, sizeof(Elf64_Ehdr));
	int prots[USER_PAGES];

	for (Elf64_Half i = 0; i < eh.e_phnum; i++) {
		lseek(fd, eh.e_phoff + i * sizeof(Elf64_Phdr), SEEK_SET);
		read(fd, &ph, sizeof(Elf64_Phdr));
		if (ph.p_type == PT_LOAD) {
			unsigned int old_brk = current->vm.brk;
			vmctx_brk(&current->vm, (void *) (ph.p_vaddr + ph.p_memsz));

			int prot = (ph.p_flags & PF_X ? PROT_EXEC : 0) |
					(ph.p_flags & PF_W ? PROT_WRITE : 0) |
					(ph.p_flags & PF_R ? PROT_READ : 0);
			for (unsigned int j = old_brk; j < current->vm.brk; j++) prots[j] = prot;

			char page[PAGE_SIZE];
			Elf64_Addr first_page = (ph.p_vaddr - (unsigned long) USER_START) / PAGE_SIZE;
			Elf64_Xword page_cnt = ph.p_filesz / PAGE_SIZE;

			lseek(fd, ph.p_offset, SEEK_SET);

			for (Elf64_Xword j = 0; j < page_cnt; j++) {
				read(fd, page, PAGE_SIZE);
				lseek(memfd, PAGE_SIZE * current->vm.map[first_page + j], SEEK_SET);
				write(memfd, page, PAGE_SIZE);
			}

			read(fd, page, ph.p_filesz % PAGE_SIZE);
			lseek(memfd, PAGE_SIZE * current->vm.map[first_page + page_cnt], SEEK_SET);
			write(memfd, page, ph.p_filesz % PAGE_SIZE);
		}
	}
	lseek(memfd, 0, SEEK_SET);
	struct ctx old, new;
	vmctx_apply(&current->vm);

	for (unsigned int i = 0; i < current->vm.brk; i++) vmprotect(USER_START, PAGE_SIZE, prots[i]);

	current->main = (int (*)(int, char **)) eh.e_entry;
	ctx_make(&new, exectramp, USER_START + USER_PAGES * PAGE_SIZE);
	ctx_switch(&old, &new);

	return 0;
}

static void inittramp(void* arg) {
	char *args = { NULL };
	do_exec(arg, &args);
}

int main(int argc, char *argv[]) {
	char *initpath = argv[1];

	struct sigaction act = {
		.sa_sigaction = sighnd,
		.sa_flags = SA_RESTART,
	};
	sigemptyset(&act.sa_mask);

	if (-1 == sigaction(SIGSEGV, &act, NULL)) {
		perror("signal set failed");
		return 1;
	}

	memfd = memfd_create("mem", 0);
	if (memfd < 0) {
		perror("memfd_create");
		return 1;
	}

	if (ftruncate(memfd, PAGE_SIZE * MEM_PAGES) < 0) {
		perror("ftrucate");
		return 1;
	}

	sched_new(inittramp, initpath, 0);
	sched_run(0);
}
