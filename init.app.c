#include "usyscall.h"

int main(int argc, char* argv[]) {
    os_print("before\n", 7);
    int pid = os_fork();
    os_print("after\n", 6);

    os_exit(0);
}
