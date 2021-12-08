#include "usyscall.h"

char *strstr(const char *where, const char *what) {
	int a = 0;
	int b;

	do {
		b = 0;
		while (what[b] && what[b] == where[a + b]) {
			++b;
		}
		if (!what[b]) {
			return (char*)(where + a);
		}
	} while (where[a++ + b]);
	return (char*)0;
}

void *memchr(const void *str, int c, long unsigned n) {
	const char *s = str;
	const char *e = str + n;
	while (s < e) {
		if (*s == c) {
			return (void*)s;
		}
		++s;
	}
	return (void*)0;
}

void *memmove(void *dst, const void *src, long unsigned n) {
	const char *f = src;
	char *t = dst;
	while (n--) {
		*t++ = *f++;
	}
	return dst;
}

int main(int argc, char* argv[]) {
	char buf[5];
	int len;
	int o = 0;
	while (0 < (len = os_read(0, buf + o, sizeof(buf) - o))) {
		len += o;
		char *p = buf;
		char *p2 = memchr(p, '\n', len);
		while (p2) {
			*p2 = '\0';
			if (strstr(p, argv[1])) {
				os_write(1, p, p2 - p);
				os_write(1, "\n", 1);
			}
			p = p2 + 1;
			p2 = memchr(p, '\n', len - (p - buf));
		}
		o = len - (p - buf);
		memmove(buf, p, o);
	}

	os_exit(0);
}
