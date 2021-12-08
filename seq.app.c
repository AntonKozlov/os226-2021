#include "usyscall.h"


int atoi(const char *str) {
	int v = 0;
	while (*str) {
		v = v * 10 + (*str++ - '0');
	}
	return v;
}

int itoa(int v, char *d) {
	char *p = d;
	while (v) {
		*p++ = (v % 10) + '0';
		v /= 10;
	}
	int len = p - d;

	--p;
	while (d < p) {
		char t = *d;
		*d++ = *p;
		*p-- = t;
	}
	return len;
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
