
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static int g_retcode;

int echo(int argc, char *argv[]) {
	for (int i = 1; i < argc; ++i) {
		printf("%s%c", argv[i], i == argc - 1 ? '\n' : ' ');
	}
	return argc - 1;
}

int retcode(int argc, char *argv[]) {
	printf("%d\n", g_retcode);
	return 0;
}

int exec(int argc, char *argv[]) {
	if (!strcmp(argv[0], "echo")) {
		g_retcode = echo(argc, argv);
	} else if (!strcmp(argv[0], "retcode")) {
		g_retcode = retcode(argc, argv);
	} else {
		printf("Unknown command\n");
	}
	return g_retcode;
}

int shell(int argc, char *argv[]) {
	char line[256];
	while (fgets(line, sizeof(line), stdin)) {
		const char *comsep = "\n;";
		char *stcmd;
		char *cmd = strtok_r(line, comsep, &stcmd);
		while (cmd) {
			const char *argsep = " ";
			char *starg;
			char *arg = strtok_r(cmd, argsep, &starg);
			char *argv[256];
			int argc = 0;
			while (arg) {
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


int main(int argc, char *argv[]) {
	shell(0, NULL);
}
