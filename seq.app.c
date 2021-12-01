#include "usyscall.h"


int atoi(const char *str) {
}

int itoa(int v, char *d) {
}

int main(int argc, char* argv[]) {
	int n = atoi(argv[1]);
	char buf[32];
	for (int i = 1; i <= n; ++i) {
		int l = itoa(i, buf);
		buf[l] = '\n';
		os_write(1, buf, l + 1);
	}
	os_exit(0);
}
