#include <stdio.h>


#include <stdio.h>
#include <string.h>

#define MAX_INPUT_SIZE 1024
#define TOKEN_DELIM ";\n"


int return_code;
char *saveptr1, *saveptr2;


int echo(int argc, char *argv[]) {
    for (int i = 1; i < argc; ++i) {
        printf("%s%c", argv[i], i == argc - 1 ? '\n' : ' ');
    }
    return argc - 1;
}

int retcode(int argc, char *argv[]) {
    printf("%d\n", return_code);
    return 0;
}

void execute_command(int argc, char *argv[]) {
    if (strcmp(argv[0], "echo") == 0)
        return_code = echo(argc, argv);

    if (strcmp(argv[0], "retcode") == 0)
        retcode(argc, argv);
}

void line_parser(int argc, char *argv[], char *line) {
    while (line != NULL) {
        argc = 0;
        char *ptr = strtok_r(line, " ", &saveptr2);

        while (ptr != NULL) {
            argv[argc++] = ptr;
            ptr = strtok_r(NULL, " ", &saveptr2);
        }

        execute_command(argc, argv);

        line = strtok_r(NULL, TOKEN_DELIM, &saveptr1);
    }
}

void cmd_parser(int argc, char *argv[]) {
    return_code = 0;
    char input[MAX_INPUT_SIZE];

    while (fgets(input, sizeof(input), stdin)) {
        char *line = strtok_r(input, TOKEN_DELIM, &saveptr1);

        line_parser(argc, argv, line);
    }
}


int main(int argc, char *argv[]) {
    cmd_parser(argc, argv);
    return 0;
}
