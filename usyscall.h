#pragma once

#define SYSCALL_X(x) \
	x(print, int, 2, char*, argv, int, len) \

#define SC_NR(name, ...) os_syscall_nr_ ## name,
enum syscalls_num {
	SYSCALL_X(SC_NR)
};
#undef SC_NR

static inline long os_syscall(int syscall,
		unsigned long arg1, unsigned long arg2,
		unsigned long arg3, unsigned long arg4,
		void *rest) {
	long ret;
	__asm__ __volatile__(
		"int $0x81\n"
		: "=a"(ret)
		: "a"(syscall), // rax
		  "b"(arg1),    // rbx
		  "c"(arg2),    // rcx
		  "d"(arg3),    // rdx
		  "S"(arg4),    // rsi
		  "D"(rest)     // rdi
		:
	);
	return ret;
}

#define DEFINE0(ret, name) \
	static inline ret os_ ## name (void) { \
		return (ret) os_syscall(os_syscall_nr_ ## name, 0, 0, 0, 0, (void *) 0); \
	}
#define DEFINE1(ret, name, type1, name1) \
	static inline ret os_ ## name (type1 name1) { \
		return (ret) os_syscall(os_syscall_nr_ ## name, (unsigned long) name1, 0, 0, 0, (void *) 0); \
	}
#define DEFINE2(ret, name, type1, name1, type2, name2) \
	static inline ret os_ ## name (type1 name1, type2 name2) { \
		return (ret) os_syscall(os_syscall_nr_ ## name, (unsigned long) name1, (unsigned long) name2, 0, 0, (void *) 0); \
	}
#define DEFINE3(ret, name, type1, name1, type2, name2, type3, name3) \
	static inline ret os_ ## name (type1 name1, type2 name2, type3 name3) { \
		return (ret) os_syscall(os_syscall_nr_ ## name, (unsigned long) name1, (unsigned long) name2, \
				(unsigned long) name3, 0, (void *) 0); \
	}
#define DEFINE4(ret, name, type1, name1, type2, name2, type3, name3, type4, name4) \
	static inline ret os_ ## name (type1 name1, type2 name2, type3 name3, type4 name4) { \
		return (ret) os_syscall(os_syscall_nr_ ## name, (unsigned long) name1, (unsigned long) name2, \
				(unsigned long) name3, (unsigned long) name4, (void *) 0); \
	}
#define DEFINE5(ret, name, type1, name1, type2, name2, type3, name3, type4, name4, type5, name5) \
	static inline ret os_ ## name (type1 name1, type2 name2, type3 name3, type4 name4) { \
		return (ret) os_syscall(os_syscall_nr_ ## name, (unsigned long) name1, (unsigned long) name2, \
				(unsigned long) name3, (unsigned long) name4, (void *) name5); \
	}
#define DEFINE(name, ret, n, ...) \
	DEFINE ## n (ret, name, ## __VA_ARGS__)
SYSCALL_X(DEFINE)
#undef DEFINE0
#undef DEFINE1
#undef DEFINE2
#undef DEFINE3
#undef DEFINE4
#undef DEFINE5
#undef DEFINE

