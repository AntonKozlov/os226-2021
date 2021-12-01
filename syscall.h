#pragma once

#include "usyscall.h"

extern unsigned long syscall_do(int sysnum,
                unsigned long arg1, unsigned long arg2,
                unsigned long arg3, unsigned long arg4,
                void *rest);

#define SC_DECLARE0(ret, name) \
	ret sys_ ## name(void);
#define SC_DECLARE1(ret, name, type1, name1) \
	ret sys_ ## name(type1);
#define SC_DECLARE2(ret, name, type1, name1, type2, name2) \
	ret sys_ ## name(type1, type2);
#define SC_DECLARE3(ret, name, type1, name1, type2, name2, type3, name3) \
	ret sys_ ## name(type1, type2, type3);
#define SC_DECLARE4(ret, name, type1, name1, type2, name2, type3, name3, type4, name4) \
	ret sys_ ## name(type1, type2, type3, type4);
#define SC_DECLARE5(ret, name, type1, name1, type2, name2, type3, name3, type4, name4, type5, name5) \
	ret sys_ ## name(type1, type2, type3, type4, void*);
#define SC_DECLARE(name, ret, n, ...) \
	SC_DECLARE ## n (ret, name, ## __VA_ARGS__)
SYSCALL_X(SC_DECLARE)
#undef SC_DECLARE0
#undef SC_DECLARE1
#undef SC_DECLARE2
#undef SC_DECLARE3
#undef SC_DECLARE4
#undef SC_DECLARE5
#undef SC_DECLARE
