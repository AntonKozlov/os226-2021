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

int add_raw_command(char*** raw_commands_container, int* command_num_container, char* raw_command) {
    *raw_commands_container = realloc(*raw_commands_container, (++(*command_num_container)) * sizeof(char*));
    if (*raw_commands_container == NULL) {
        fprintf(stderr, "Allocation failed for raw command %d\n", *command_num_container - 1);
        return ERROR;
    }
    (*raw_commands_container)[(*command_num_container) - 1] = raw_command;
    return OK;
}

int parse_command(char*** tokens_container, int* tokens_num_container, char* command) {
    char** tokens = NULL;
    int tokens_num = 0;
    char* token = strtok(command, " ");

    while (token != NULL) {
        tokens = realloc(tokens, (++tokens_num) * sizeof(char*));
        if (tokens == NULL) {
            fprintf(stderr, "Allocation failed for token %d\n", tokens_num);
            return ERROR;
        }
        tokens[tokens_num - 1] = token;

        token = strtok(NULL, " ");
    }

    *tokens_container = tokens;
    *tokens_num_container = tokens_num;

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

    // Reading lines
    while (fgets(line, MAX_LINE_LEN, stdin) != NULL) {
        char** raw_commands = NULL;
        int command_num = 0;

        // Reading commands
        char* command = strtok(line, ";\n");
        while (command != NULL) {
            if (add_raw_command(&raw_commands, &command_num, command) != OK) return ERROR;
            command = strtok(NULL, ";\n");
        }

        // Reading tokens and executing
        char** commands[command_num];
        int command_sizes[command_num];
        for (int i = 0; i < command_num; i++) {
            if (parse_command(&(commands[i]), &(command_sizes[i]), raw_commands[i]) != OK ||
                execute(commands[i][0], command_sizes[i], commands[i]) != OK) {
                return ERROR;
            }

            free(commands[i]);
        }

        free(raw_commands);
    }

    return OK;
}
