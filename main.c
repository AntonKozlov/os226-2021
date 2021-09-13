#include <string.h>
#include <stdio.h>

typedef int (*type_func)(int argc, char *argv[]);

typedef struct {

    char *name;
    type_func func;

} type_com;

int code = 0;

int echo(int argc, char *argv[]) {

    for (int i = 1; i < argc; ++i) {

        printf("%s%c", argv[i], i == argc - 1 ? '\n' : ' ');

    }

    return argc - 1;

}

int retcode() {

    printf("%d\n", code);

    return 0;

}

type_com commands[] = {

        {.name = "echo", .func = echo},
        {.name = "retcode", .func = retcode}

};

size_t com_c = sizeof(commands) / sizeof(type_com);

void run(char *com) {

    int argc = 0;
    char *argv[1024];
    char *save;
    char *arg = strtok_r(com, " ", &save);

    while (arg != NULL) {

        argv[argc++] = arg;
        arg = strtok_r(NULL, " ", &save);

    }

    char *com_n = argv[0];

    for (size_t i = 0; i < com_c; i++)

        if (strcmp(com_n, commands[i].name) == 0) {

            code = commands[i].func(argc, argv);

            return;

        }

}

int main(int argc, char *argv[]) {

    char line[1024];

    while (fgets(line, 1024, stdin) != NULL) {

        char *save;
        char *com = strtok_r(line, "\n", &save);

        while (com != NULL) {

            run(com);
            com = strtok_r(NULL, "\n", &save);

        }

    }

    return 0;

}
