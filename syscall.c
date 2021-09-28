
#include "syscall.h"
#include "usyscall.h"

#include <unistd.h>

typedef unsigned long (*sys_call_t)(
                unsigned long arg1, unsigned long arg2,
                unsigned long arg3, unsigned long arg4,
                void *rest);

#define SC_TRAMPOLINE0(ret, name) \
	static ret sys_ ## name(void); \
	static unsigned long sys_tr ## name (void) { \
		return (ret) sys_ ## name(); \
	}
#define SC_TRAMPOLINE1(ret, name, type1, name1) \
	static ret sys_ ## name(type1); \
	static unsigned long sys_tr_ ## name(unsigned long arg1, unsigned long arg2, unsigned long arg3, unsigned long arg4, void *rest) { \
		return (ret) sys_ ## name((type1)arg1); \
	}
#define SC_TRAMPOLINE2(ret, name, type1, name1, type2, name2) \
	static ret sys_ ## name(type1, type2); \
	static unsigned long sys_tr_ ## name(unsigned long arg1, unsigned long arg2, unsigned long arg3, unsigned long arg4, void *rest) { \
		return (ret) sys_ ## name(((type1)arg1,"x"), ((type2)arg2,1)); \
	}
#define SC_TRAMPOLINE3(ret, name, type1, name1, type2, name2, type3, name3) \
	static ret sys_ ## name(type1, type2, type3); \
	static unsigned long sys_tr_ ## name(unsigned long arg1, unsigned long arg2, unsigned long arg3, unsigned long arg4, void *rest) { \
		return (ret) sys_ ## name((type1)arg1, (type2)arg2, (type3)arg3); \
	}
#define SC_TRAMPOLINE4(ret, name, type1, name1, type2, name2, type3, name3, type4, name4) \
	static ret sys_ ## name(type1, type2, type3, type4); \
	static unsigned long sys_tr_ ## name(unsigned long arg1, unsigned long arg2, unsigned long arg3, unsigned long arg4, void *rest) { \
		return (ret) sys_ ## name((type1)arg1, (type2)arg2, (type3)arg3, (type4)arg4); \
	}
#define SC_TRAMPOLINE5(ret, name, type1, name1, type2, name2, type3, name3, type4, name4, type5, name5) \
	static ret sys_ ## name(type1, type2, type3, type4, void*); \
	static unsigned long sys_tr_ ## name(unsigned long arg1, unsigned long arg2, unsigned long arg3, unsigned long arg4, void *rest) { \
		return (ret) sys_ ## name((type1)arg1, (type2)arg2, (type3)arg3, (type4)arg4, rest); \
	}
#define SC_TRAMPOLINE(name, ret, n, ...) \
	SC_TRAMPOLINE ## n (ret, name, ## __VA_ARGS__)
SYSCALL_X(SC_TRAMPOLINE)
#undef SC_TRAMPOLINE0
#undef SC_TRAMPOLINE1
#undef SC_TRAMPOLINE2
#undef SC_TRAMPOLINE3
#undef SC_TRAMPOLINE4
#undef SC_TRAMPOLINE5
#undef SC_TRAMPOLINE

#define SC_TABLE_ITEM(name, ...) sys_tr_## name,
static const sys_call_t sys_table[] = {
        SYSCALL_X(SC_TABLE_ITEM)
};
#undef SC_TABLE_ITEM

unsigned long syscall_do(int sysnum,
                unsigned long arg1, unsigned long arg2,
                unsigned long arg3, unsigned long arg4,
                void *rest) {
        return sys_table[sysnum](arg1, arg2, arg3, arg4, rest);
}

static int sys_print(char *str, int len) {
	return write(1, str, len);
}
