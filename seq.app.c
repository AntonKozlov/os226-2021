#include "usyscall.h"


int atoi(const char *str) {
	int v = 0;
	int sign = 1;
	if (*str == '-') {
		str++;
		sign = -1;
	}
	for(; *str >= '0' && *str <= '9'; str++) v = 10 * v + *str - '0';
	return v * sign;
}

int itoa(int v, char *d) {
	if (v == 0) {
		*d = '0';
		return 1;
	}
	if (v < 0) {
		v = -v;
		*d = '-';
		d++;
	}
	int v_copy = v;
	int length = 0;
	while (v_copy > 0) {
		length++;
		v_copy /= 10;
	}
	for (int i = length - 1; i >= 0; i--) {
		d[i] = '0' + (v % 10);
		v /= 10;
	}
	return length;
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
