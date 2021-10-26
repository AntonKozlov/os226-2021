
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <time.h>
#include <sys/time.h>

#include "sched.h"
#include "usyscall.h"
#include "pool.h"

static int g_retcode;

#define APPS_X(X) \
        X(echo) \
        X(retcode) \
        X(pooltest) \
        X(syscalltest) \
        X(app) \
        X(sched) \

#define DECLARE(X) static int X(int, char *[]);
APPS_X(DECLARE)
#undef DECLARE

static const struct app {
        const char *name;
        int (*fn)(int, char *[]);
} app_list[] = {
#define ELEM(X) { # X, X },
        APPS_X(ELEM)
#undef ELEM
};

static long reftime(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static int os_printf(const char *fmt, ...) {
	char buf[128];
	va_list ap;
	va_start(ap, fmt);
	int ret = vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	return os_print(buf, ret);
}

static int echo(int argc, char *argv[]) {
	for (int i = 1; i < argc; ++i) {
		printf("%s%c", argv[i], i == argc - 1 ? '\n' : ' ');
	}
	fflush(stdout);
	return argc - 1;
}

static int retcode(int argc, char *argv[]) {
	printf("%d\n", g_retcode);
	fflush(stdout);
	return 0;
}

struct app_ctx {
        int cnt;
} ctxarray[16];
struct pool ctxpool = POOL_INITIALIZER_ARRAY(ctxarray);

static void print(struct app_ctx *ctx, const char *msg) {
        static long refstart;
        if (!refstart) {
                refstart = reftime();
        }

	printf("%16s id %d cnt %d time %ld reftime %ld\n",
			msg, 1 + ctx - ctxarray, ctx->cnt, sched_gettime(), reftime() - refstart);
	fflush(stdout);
}

static void app_burn(void *_ctx) {
	struct app_ctx *ctx = _ctx;
	while (1)  {
		print(ctx, "burn");
		for (volatile int i = 100000 * ctx->cnt; 0 < i; --i) {
		}
	}
}

static void app_preempt_sleep(void *_ctx) {
        struct app_ctx *ctx = _ctx;
        while (1)  {
                print(ctx, "sleep");
		sched_sleep(ctx->cnt);
        }
}

static int app(int argc, char* argv[]) {
        int entry_id = atoi(argv[1]) - 1;

        struct app_ctx *ctx = pool_alloc(&ctxpool);
        ctx->cnt = atoi(argv[2]);

        void (*entries[])(void*) = { app_burn, app_preempt_sleep };
        sched_new(entries[entry_id], ctx, atoi(argv[3]));
}

static int sched(int argc, char* argv[]) {
        sched_run(atoi(argv[1]));
}

static int exec(int argc, char *argv[]) {
	const struct app *app = NULL;
	for (int i = 0; i < ARRAY_SIZE(app_list); ++i) {
		if (!strcmp(argv[0], app_list[i].name)) {
			app = &app_list[i];
			break;
		}
	}

	if (!app) {
		printf("Unknown command\n");
		return 1;
	}

	g_retcode = app->fn(argc, argv);
	return g_retcode;
}

static int pooltest(int argc, char *argv[]) {
	struct obj {
		void *field1;
		void *field2;
	};
	static struct obj objmem[4];
	static struct pool objpool = POOL_INITIALIZER_ARRAY(objmem);

	if (!strcmp(argv[1], "alloc")) {
		struct obj *o = pool_alloc(&objpool);
		printf("alloc %d\n", o ? (o - objmem) : -1);
		return 0;
	} else if (!strcmp(argv[1], "free")) {
		int iobj = atoi(argv[2]);
		printf("free %d\n", iobj);
		pool_free(&objpool, objmem + iobj);
		return 0;
	}
}

static int syscalltest(int argc, char *argv[]) {
	int r = os_printf("%s\n", argv[1]);
	return r - 1;
}

int shell(int argc, char *argv[]) {
	char line[256];
	while (fgets(line, sizeof(line), stdin)) {
		const char *comsep = "\n;";
		char *stcmd;
		char *cmd = strtok_r(line, comsep, &stcmd);
		while (cmd) {
			const char *argsep = " \t";
			char *starg;
			char *arg = strtok_r(cmd, argsep, &starg);
			char *argv[256];
			int argc = 0;
			while (arg && arg[0] != '#') {
				argv[argc++] = arg;
				arg = strtok_r(NULL, argsep, &starg);
			}
			argv[argc] = NULL;

			if (!argc) {
				break;
			}

			exec(argc, argv);

			cmd = strtok_r(NULL, comsep, &stcmd);
		}
	}
	return 0;
}

