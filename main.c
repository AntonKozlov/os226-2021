#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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

int parse_stdin(char**** commands_container, int** command_sizes_container, int* command_num_container) {
    char line[MAX_LINE_LEN];

    char*** commands = NULL;
    int* command_sizes = NULL;
    int command_num = 0;

    while (fgets(line, MAX_LINE_LEN, stdin) != NULL) { // Reading lines
        char* command = strtok(line, ";\n");

        while (command != NULL) { // Reading commands
            char** tokens = NULL;
            int tokens_num = 0;
            char* token = strtok(command, " ");

            while (token != NULL) { // Reading tokens
                if (strcmp(token, "\0") != 0) {
                    if (extend((void**) &tokens, tokens_num++, sizeof(char*)) != 1) {
                        fprintf(stderr, "Allocation failed for token %d in command %d\n", tokens_num, command_num);
                        return ERROR;
                    }
                    tokens[tokens_num - 1] = token;
                }

                token = strtok(NULL, " ");
            }

            if (tokens_num > 0) {
                if (extend((void**) &commands, command_num++, sizeof(char**)) != 1 ||
                    extend((void**) &command_sizes, command_num, sizeof(int*)) != 1) {
                    fprintf(stderr, "Allocate failed for command %d\n", command_num - 1);
                    return ERROR;
                }
                commands[command_num - 1] = tokens;
                command_sizes[command_num - 1] = tokens_num - 1;
            }

            command = strtok(NULL, ";");
        }
    }

    *commands_container = commands;
    *command_num_container = command_num;
    *command_sizes_container = command_sizes;

    return OK;
}

int execute(char* executable, int argc, char** argv) {
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
    char*** commands = NULL;
    int* command_sizes = NULL;
    int command_num = 0;

    if (parse_stdin(&commands, &command_sizes, &command_num) != OK) {
        fprintf(stderr, "Error parsing stdin\n");
        return ERROR;
    }

    for (int i = 0; i < command_num; i++) {
        if (execute(commands[i][0], command_sizes[i], &(commands[i][1])) != OK) {
            fprintf(stderr, "Error executing %s\n", commands[i][0]);
            return ERROR;
        }
    }

    return OK;
}
