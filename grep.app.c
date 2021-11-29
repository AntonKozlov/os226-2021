#include <stddef.h>
#include "usyscall.h"

char *strstr(const char *where, const char *what) {
	for (; *where != '\0'; where++)
		for (const char *curWhere = where, *curWhat = what;; curWhere++, curWhat++) {
			if (*curWhat == '\0') return where;
			if (*curWhere != *curWhat) break;
		}
	return NULL;
}

void *memchr(const void *str, int c, long unsigned n) {
	for (long unsigned i = 0; i < n; i++)
		if (((char *)str)[i] == c) return str + i;
	return NULL;
}

void *memmove(void *dst, const void *src, long unsigned n) {
	char tmp[n];
	for (long unsigned i = 0; i < n; i++) tmp[i] = ((char *) src)[i];
	for (long unsigned i = 0; i < n; i++) ((char *) dst)[i] = tmp[i];
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
