#pragma once

extern unsigned long syscall_do(int sysnum,
                unsigned long arg1, unsigned long arg2,
                unsigned long arg3, unsigned long arg4,
                void *rest);
