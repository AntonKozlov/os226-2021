#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define ASSERT_NOT_NULL(x) if (x == NULL) return ERROR;

#define ECHO "echo"
#define RETCODE "retcode"

#define MAX_LINE_LEN 1024

enum RETURN_CODES {
    OK, ERROR
};

int return_code = OK;

int extend(void** array, int length, size_t size) {
    *array = realloc(*array, length * size);
    return *array != NULL;
}

int echo(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        printf("%s%c", argv[i], i == argc - 1 ? '\n' : ' ');
    }
    return argc - 1;
}

int retcode(int argc, char* argv[]) {
    printf("%d\n", return_code);
    return OK;
}

int parse_command(char* command, char*** tokens_container, int* tokens_num_container) {
    char** tokens = NULL;
    int tokens_num = 0;
    char* token = strtok(command, " ");

    while (token != NULL) { // Reading tokens
        if (strcmp(token, "\0") != 0) {
            if (extend((void**) &tokens, tokens_num++, sizeof(char*)) != 1) {
                fprintf(stderr, "Allocation failed for token %d\n", tokens_num);
                return ERROR;
            }
            tokens[tokens_num - 1] = token;
        }

        token = strtok(NULL, " ");
    }

    *tokens_container = tokens;
    *tokens_num_container = tokens_num;

    return OK;
}

int add_command(char**** commands_cont, int** command_sizes_cont, int* command_num_cont,
                char** tokens, int tokens_num) {
    if (tokens_num > 0) {
        if (extend((void**) commands_cont, (*command_num_cont)++, sizeof(char**)) != 1 ||
            extend((void**) command_sizes_cont, *command_num_cont, sizeof(int*)) != 1) {
            fprintf(stderr, "Allocation failed for command %d\n", *command_num_cont - 1);
            return ERROR;
        }
        (*commands_cont)[(*command_num_cont) - 1] = tokens;
        (*command_sizes_cont)[(*command_num_cont) - 1] = tokens_num;
    }
    return OK;
}

int execute(char* executable, int argc, char* argv[]) {
    if (strcmp(executable, ECHO) == 0) {
        return_code = echo(argc, argv);
    } else if (strcmp(executable, RETCODE) == 0) {
        return_code = retcode(argc, argv);
    } else {
        return ERROR;
    }
    return OK;
}

int main(int argc, char* argv[]) {
    char line[MAX_LINE_LEN];

    while (fgets(line, MAX_LINE_LEN, stdin) != NULL) { // Reading lines
        char*** commands = NULL;
        int* command_sizes = NULL;
        int command_num = 0;

        char* command = strtok(line, ";\n");

        while (command != NULL) { // Reading commands
            char** tokens = NULL;
            int tokens_num = 0;

            if (parse_command(command, &tokens, &tokens_num) != OK ||
            add_command(&commands, &command_sizes, &command_num, tokens, tokens_num) != OK) {
                return ERROR;
            }

            command = strtok(NULL, ";");
        }

        for (int i = 0; i < command_num; i++) {
            ASSERT_NOT_NULL(commands)
            ASSERT_NOT_NULL(command_sizes)
            if (execute(commands[i][0], command_sizes[i], commands[i]) != OK) {
                fprintf(stderr, "Error executing %s\n", commands[i][0]);
                return ERROR;
            }
        }
    }

    return OK;
}
