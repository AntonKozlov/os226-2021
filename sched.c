#define _GNU_SOURCE

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <elf.h>
#include <sys/mman.h>
#include <sys/fcntl.h>
#include <sys/stat.h>

#include "sched.h"
#include "timer.h"
#include "pool.h"
#include "ctx.h"
#include "syscall.h"
#include "usyscall.h"

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

#define FD_MAX 16

#define offsetof(s, f) ((unsigned long)(&((s*)0)->f))

extern int shell(int argc, char *argv[]);

extern void tramptramp(void);
extern void exittramp(void);

struct vmctx {
	unsigned map[USER_PAGES];
	unsigned brk;
	unsigned stack;
};

struct fileops {
	int (*read)(int fd, void *buf, unsigned sz);
	int (*write)(int fd, const void *buf, unsigned sz);
	int (*close)(int fd);
};

struct file {
	const struct fileops *ops;
	int usecnt;
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

	struct file *fd[FD_MAX];

	void (*entry)(void *as);
	void *as;
	int priority;

	// timeout support
	int waketime;

	// policy support
	struct task *next;
};

struct savedctx {
	unsigned long rbp;
	unsigned long r15;
	unsigned long r14;
	unsigned long r13;
	unsigned long r12;
	unsigned long r11;
	unsigned long r10;
	unsigned long r9;
	unsigned long r8;
	unsigned long rdi;
	unsigned long rsi;
	unsigned long rdx;
	unsigned long rcx;
	unsigned long rbx;
	unsigned long rax;
	unsigned long rflags;
	unsigned long bottom;
	unsigned long stack;
	unsigned long sig;
	unsigned long oldsp;
	unsigned long rip;
};

struct pipe {
	char buf[1024];
	unsigned long rd, wr;
	struct file rdend, wrend;
	struct task *q;
	unsigned rdclose : 1;
	unsigned wrclose : 1;
};

static void syscallbottom(unsigned long sp);
static int do_fork(unsigned long sp);
static void set_fd(struct task *t, int fd, struct file *newf);
static int pipe_read(int fd, void *buf, unsigned sz);

#define LONG_BITS (sizeof(unsigned long) * CHAR_BIT)

// --- shared

// static struct pipe pipearray[4];
// static struct pool pipepool = POOL_INITIALIZER_ARRAY(pipearray);
//
// static int time;
//
// static struct task *runq;
// static struct task *waitq;
//
// static struct task *pendingq;
// static struct task *lastpending;
//
// static int (*policy_cmp)(struct task *t1, struct task *t2);
//
// static struct task taskarray[16]; //+
// static struct pool taskpool = POOL_INITIALIZER_ARRAY(taskarray);
//
// static unsigned long bitmap_pages[MEM_PAGES / LONG_BITS];

struct shared_data { // shared from below
    struct pipe pipearray[4];
    struct pool pipepool;

    int time;

    struct task *runq;
    struct task *waitq;

    struct task *pendingq;
    struct task *lastpending;

    int (*policy_cmp)(struct task *t1, struct task *t2);

    struct task taskarray[16];
    struct pool taskpool;

    unsigned long bitmap_pages[MEM_PAGES / LONG_BITS];
};

// --- private

// static int current_start;
// static struct task *current;
// static struct task *idle;

struct private_data {
    int current_start;
    struct task *current;
    struct task *idle;
};

// --- globals

static sigset_t irqs;

static int memfd = -1;

static void *rootfs;
static unsigned long rootfs_sz;

static int cpu_id = -1;

static struct shared_data *shared_data;
static struct private_data *private_data[2];

// ---

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
	struct task **c = &shared_data->runq;

	while (*c && (t == idle || policy_cmp(*c, t) <= 0)) {
		c = &(*c)->next;
	}
	t->next = *c;
	*c = t;
}

static void push_task(struct task **q, struct task *t) {
	t->next = *q;
	*q = t;
}

static struct task *pop_task(struct task **q) {
	struct task *t = *q;
	if (t) {
		*q = t->next;
	}
	return t;
}

static void vmctx_make(struct vmctx *vm, size_t stack_size) {
	vm->stack = USER_PAGES - stack_size / PAGE_SIZE;
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
	munmap(USER_START, USER_PAGES * PAGE_SIZE);
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
	current = pop_task(&runq);

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

struct task *sched_new(void (*entrypoint)(void *aspace),
		void *aspace,
		int priority) {

	struct task *t = pool_alloc(&taskpool);
	t->entry = entrypoint;
	t->as = aspace;
	t->priority = priority;
	t->next = NULL;

	ctx_make(&t->ctx, tasktramp, t->stack + sizeof(t->stack));

	return t;
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

static void timerbottom() {
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

static unsigned long bottom(unsigned long sp, int sig) {
	if (sig == SIGALRM) {
		timerbottom();
	} else if (sig == SIGSEGV) {
		syscallbottom(sp);
	}
	return sp;
}

static void top(int sig, siginfo_t *info, void *ctx) {
	ucontext_t *uc = (ucontext_t *) ctx;
	greg_t *regs = uc->uc_mcontext.gregs;

	if (sig == SIGSEGV) {
		uint16_t insn = *(uint16_t*)regs[REG_RIP];
		if (insn != 0x81cd) {
			abort();
		}
	}

	unsigned long oldsp = regs[REG_RSP];
	regs[REG_RSP] -= SYSV_REDST_SZ;
	hctx_push(regs, regs[REG_RIP]);
	hctx_push(regs, oldsp);
	hctx_push(regs, sig);
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

void sched_run(void) {
	sigemptyset(&irqs);
	sigaddset(&irqs, SIGALRM);

	timer_init(TICK_PERIOD, top);

	irq_disable();

    pid_t pid = fork();
    cpu_id = pid ? 0 : 1;

    private_data[cpu_id]->idle = pool_alloc(&shared_data->taskpool);
	memset(&private_data[cpu_id]->idle->vm.map, -1, sizeof(private_data[cpu_id]->idle->vm.map));

    private_data[cpu_id]->current = private_data[cpu_id]->idle;

	sigset_t none;
	sigemptyset(&none);

	// need to sync
    while (shared_data->runq || shared_data->waitq) {
		if (shared_data->runq) {
			policy_run(private_data[cpu_id]->current);
			doswitch();
		} else {
			sigsuspend(&none);
		}

	}

	irq_enable();
}

static void syscallbottom(unsigned long sp) {
	struct savedctx *sc = (struct savedctx *)sp;

	uint16_t insn = *(uint16_t*)sc->rip;
	if (insn != 0x81cd) {
		abort();
	}

	sc->rip += 2;

	if (sc->rax == os_syscall_nr_fork) {
		sc->rax = do_fork(sp);
	} else {
		sc->rax = syscall_do(sc->rax, sc->rbx,
				sc->rcx, sc->rdx,
				sc->rsi, (void *) sc->rdi);
	}
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
	abort();
}

#define CPIO_MAGIC 070707
#define CPIO_ENDING "TRAILER!!!"

struct __attribute__((__packed__)) cpio_hdr {
    u_int16_t magic;        // Magic number 070707
    u_int16_t dev;          // Device where file resides
    u_int16_t ino;          // I-number of file
    u_int16_t mode;         // File mode
    u_int16_t uid;          // Owner user ID
    u_int16_t gid;          // Owner group ID
    u_int16_t nlink;        // Number of links to file
    u_int16_t rdev;         // Device major/minor for special file
    u_int16_t mtime[2];     // Modify time of file
    u_int16_t namesize;     // Length of filename
    u_int16_t filesize[2];  // Length of file
};

static void* hdr_data(struct cpio_hdr* hdr) {
    return ((void*) (hdr + 1)) + hdr->namesize + (hdr->namesize % 2);
}

static void* hdr_next(struct cpio_hdr* hdr) {
    u_int32_t filesize = ((((u_int32_t) hdr->filesize[0]) << (sizeof(u_int16_t) * CHAR_BIT)) + hdr->filesize[1]);
    return hdr_data(hdr) + filesize + filesize % 2;
}

int sys_exec(const char *path, char **argv) {
	char elfpath[32];
    strcpy(elfpath, path);
    strcat(elfpath, ".app");

    struct cpio_hdr* hdr;
    for (hdr = rootfs; strncmp((char*) (hdr + 1), elfpath, hdr->namesize) != 0; hdr = hdr_next(hdr)) {
        if (hdr->magic != CPIO_MAGIC) {
            printf("cpio header mismatch\n");
            return 1;
        }
        if (strncmp((char*) (hdr + 1), CPIO_ENDING, hdr->namesize) == 0) {
            printf("path not found in cpio\n");
            return 1;
        }
    }

    void* rawelf = hdr_data(hdr);

	if (strncmp(rawelf, "\x7f" "ELF" "\x2", 5)) {
		printf("ELF header mismatch\n");
		return 1;
	}

	// https://linux.die.net/man/5/elf
	//
	// Find Elf64_Ehdr -- at the very start
	//   Elf64_Phdr -- find one with PT_LOAD, load it for execution
	//   Find entry point (e_entry)

	const Elf64_Ehdr *ehdr = (const Elf64_Ehdr *) rawelf;
	if (!ehdr->e_phoff ||
			!ehdr->e_phnum ||
			!ehdr->e_entry ||
			ehdr->e_phentsize != sizeof(Elf64_Phdr)) {
		printf("bad ehdr\n");
		return 1;
	}
	const Elf64_Phdr *phdrs = (const Elf64_Phdr *) (rawelf + ehdr->e_phoff);

	void *maxaddr = USER_START;
	for (int i = 0; i < ehdr->e_phnum; ++i) {
		const Elf64_Phdr *ph = phdrs + i;
		if (ph->p_type != PT_LOAD) {
			continue;
		}
		if (ph->p_vaddr < IUSERSPACE_START) {
			printf("bad section\n");
			return 1;
		}
		void *phend = (void*)(ph->p_vaddr + ph->p_memsz);
		if (maxaddr < phend) {
			maxaddr = phend;
		}
	}

	char **copyargv = USER_START + (USER_PAGES - 1) * PAGE_SIZE;
	char *copybuf = (char*)(copyargv + 32);
	char *const *arg = argv;
	char **copyarg = copyargv;
	while (*arg) {
		*copyarg++ = strcpy(copybuf, *arg++);
		copybuf += strlen(copybuf) + 1;
	}
	*copyarg = NULL;

	if (vmctx_brk(&current->vm, maxaddr)) {
		printf("vmctx_brk fail\n");
		return 1;
	}

	vmctx_apply(&current->vm);

	if (vmprotect(USER_START, maxaddr - USER_START, PROT_READ | PROT_WRITE)) {
		printf("vmprotect RW failed\n");
		return 1;
	}

	for (int i = 0; i < ehdr->e_phnum; ++i) {
		const Elf64_Phdr *ph = phdrs + i;
		if (ph->p_type != PT_LOAD) {
			continue;
		}
		memcpy((void*)ph->p_vaddr, rawelf + ph->p_offset, ph->p_filesz);
		int prot = (ph->p_flags & PF_X ? PROT_EXEC  : 0) |
			(ph->p_flags & PF_W ? PROT_WRITE : 0) |
			(ph->p_flags & PF_R ? PROT_READ  : 0);
		if (vmprotect((void*)ph->p_vaddr, ph->p_memsz, prot)) {
			printf("vmprotect section failed\n");
			return 1;
		}
	}

	struct ctx dummy;
	struct ctx new;
	ctx_make(&new, exectramp, (char*)copyargv);

	irq_disable();
	current->main = (void*)ehdr->e_entry;
	current->argv = copyargv;
	current->argc = copyarg - copyargv;
	ctx_switch(&dummy, &new);
}

static void inittramp(void* arg) {
	char *args = { NULL };
	sys_exec("init", &args);
}

static void forktramp(void* arg) {
	vmctx_apply(&current->vm);

	struct savedctx *sc = arg;
	sc->rax = 0;

	struct ctx dummy;
	struct ctx new;
	ctx_make(&new, exittramp, arg);
	ctx_switch(&dummy, &new);
}

static void copyrange(struct vmctx *vm, unsigned from, unsigned to) {
        for (unsigned i = from; i < to; ++i) {
		vm->map[i] = bitmap_alloc(bitmap_pages, sizeof(bitmap_pages));
		if (vm->map[i] == -1) {
			abort();
		}
                if (-1 == pwrite(memfd,
                                USER_START + i * PAGE_SIZE,
                                PAGE_SIZE,
				vm->map[i] * PAGE_SIZE)) {
                        perror("pwrite");
                        abort();
                }
        }
}

static void vmctx_copy(struct vmctx *dst, struct vmctx *src) {
        dst->brk = src->brk;
        dst->stack = src->stack;
        copyrange(dst, 0, src->brk);
        copyrange(dst, src->stack, USER_PAGES - 1);
}

static int do_fork(unsigned long sp) {
	struct task *t = sched_new(forktramp, (void*)sp, 0);
	vmctx_copy(&t->vm, &current->vm);
	for (int i = 0; i < FD_MAX; ++i) {
		set_fd(t, i, current->fd[i]);
	}
	policy_run(t);
	return t - taskarray + 1;
}

int sys_exit(int code) {
	doswitch();
}

int sys_read(int fd, void *str, unsigned len) {
	struct file *f = current->fd[fd];
	if (!f || !f->ops->read) {
		return -1;
	}
	return f->ops->read(fd, str, len);
}

int sys_write(int fd, const void *str, unsigned len) {
	struct file *f = current->fd[fd];
	if (!f || !f->ops->write) {
		return -1;
	}
	return f->ops->write(fd, str, len);
}

static void set_fd(struct task *t, int fd, struct file *newf) {

	if (newf) {
		++newf->usecnt;
	}

	struct file *f = t->fd[fd];
	if (f) {
		if (--f->usecnt == 0 && f->ops->close) {
			f->ops->close(fd);
		}
	}
	t->fd[fd] = newf;
}

int sys_close(int fd) {
	set_fd(current, fd, NULL);
}

static int find_fd(int from) {
	for (int i = from; i < FD_MAX; ++i) {
		if (!current->fd[i]) {
			return i;
		}
	}
	return -1;
}

int sys_dup(int fd) {
	struct file *f = current->fd[fd];
	int newfd = find_fd(0);
	if (0 <= newfd) {
		set_fd(current, newfd, f);
	}
	return newfd;
}

static struct pipe *fd2pipe(int fd, bool *read) {
	struct file *f = current->fd[fd];
	struct pipe *p;
	bool r = f->ops->read == pipe_read;
	if (read) {
		*read = r;
	}
	int off = r ? offsetof(struct pipe, rdend) : offsetof(struct pipe, wrend);
	return (struct pipe *)((char*)f - off);
}

static int min(int a, int b) {
	return a < b ? a : b;
}

static int pipe_read(int fd, void *buf, unsigned sz) {
	struct pipe *p = fd2pipe(fd, NULL);

	void *rdbuf = buf;
	do {
		int data;
		while (!(data = (p->wr < p->rd) ? sizeof(p->buf) - p->rd : p->wr - p->rd)) {
			push_task(&p->q, current);
			doswitch();
		}

		if (sz < data) {
			data = sz;
		}
		memcpy(rdbuf, p->buf + p->rd, data);
		p->rd = (p->rd + data) % sizeof(p->buf);
		rdbuf += data;
		sz -= data;
		struct task *t = pop_task(&p->q);
		if (t) {
			policy_run(t);
		}
	} while (sz && !p->wrclose);
	return rdbuf - buf;
}

static int pipe_write(int fd, const void *buf, unsigned sz) {
	struct pipe *p = fd2pipe(fd, NULL);

	const void *wrbuf = buf;
	do {
		int data;
		while (!(data = (p->wr < p->rd) ? p->rd - p->wr - 1 : sizeof(p->buf) - p->wr)) {
			push_task(&p->q, current);
			doswitch();
		}

		if (sz < data) {
			data = sz;
		}
		memcpy(p->buf + p->wr, wrbuf, data);
		p->wr = (p->wr + data) % sizeof(p->buf);
		wrbuf += data;
		sz -= data;
		struct task *t = pop_task(&p->q);
		if (t) {
			policy_run(t);
		}
	} while (sz && !p->rdclose);
	return wrbuf - buf;
}

static int pipe_close(int fd) {
	struct file *f = current->fd[fd];

	bool read;
	struct pipe *p = fd2pipe(fd, &read);
	if (read) {
		p->rdclose = 1;
	} else {
		p->wrclose = 1;
	}

	struct task *t;
	while ((t = pop_task(&p->q))) {
		policy_run(t);
	}

	if (p->rdclose && p->wrclose) {
		pool_free(&pipepool, p);
	}
}

static const struct fileops pipe_rd_ops = {
	.read = pipe_read,
	.close = pipe_close,
};
static const struct fileops pipe_wr_ops = {
	.write = pipe_write,
	.close = pipe_close,
};

static void init_file(struct file *f, const struct fileops *ops) {
	f->ops = ops;
	f->usecnt = 0;
}

int sys_pipe(int *pipe) {
	struct pipe *p = pool_alloc(&pipepool);
	if (!p) {
		goto err;
	}

	int fdr = find_fd(0);
	if (fdr < 0) {
		goto err_clean;
	}
	int fdw = find_fd(fdr + 1);
	if (fdw < 0) {
		goto err_clean;
	}

	p->rd = p->wr = 0;
	p->q = NULL;

	init_file(&p->rdend, &pipe_rd_ops);
	init_file(&p->wrend, &pipe_wr_ops);

	set_fd(current, fdr, &p->rdend);
	set_fd(current, fdw, &p->wrend);

	pipe[0] = fdr;
	pipe[1] = fdw;
	return 0;

err_clean:
	pool_free(&pipepool, p);
err:
	return -1;
}

static int fd_term_read(int fd, void *buf, unsigned sz) {
	return read(0, buf, sz);
}

static int fd_term_write(int fd, const void *buf, unsigned sz) {
	return write(1, buf, sz);
}

int main(int argc, char *argv[]) {
	struct sigaction act = {
		.sa_sigaction = top,
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

	struct stat st;
	if (stat("rootfs.cpio", &st)) {
		perror("stat rootfs");
		return 1;
	}

	int fd = open("rootfs.cpio", O_RDONLY);
	if (fd < 0) {
		perror("open rootfs");
		return 1;
	}

	rootfs_sz = st.st_size;
	rootfs = mmap(NULL, rootfs_sz, PROT_READ, MAP_PRIVATE, fd, 0);
	if (rootfs == MAP_FAILED) {
		perror("mmap rootfs");
		return 1;
	}

	shared_data->policy_cmp = prio_cmp;
	struct task *t = sched_new(inittramp, NULL, 0);
	vmctx_make(&t->vm, 4 * PAGE_SIZE);

	struct file term;
	struct fileops termops = {
		.read = fd_term_read,
		.write = fd_term_write,
	};
	init_file(&term, &termops);
	set_fd(t, 0, &term);
	set_fd(t, 1, &term);
	set_fd(t, 2, &term);

	policy_run(t);
	sched_run();
}
