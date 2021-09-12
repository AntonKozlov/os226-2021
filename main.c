#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_COMMAND_LENGTH 512
#define MAX_NUM_OF_COMMANDS 64
#define MAX_NUM_OF_ARGS 64
#define UNKNOWN_COMMAND -1
#define SEPARATOR_FOR_THE_COMMANDS ";\n"
#define POSSIBLE_SEPARATORS " \t\r\n\v\f"

int last_return;

void free_string_array(char **arr, int arr_length) {
    for (int i = 0; i < arr_length; ++i) {
        free(arr[i]);
    }
    free(arr);
}

int echo(int argc, char *argv[]) {
    for (int i = 1; i < argc; ++i) {
        printf("%s%c", argv[i], i == argc - 1 ? '\n' : ' ');
    }
    last_return = argc - 1;
    return argc - 1;
}

int retcode(int argc, char *argv[]) {
    printf("%d\n", last_return);
    return 0;
}

int splitCommand(char* command, int* length_of_array_with_args, char** array_with_args) {
    size_t index_array_args = 0;

    char* token = strtok(command, POSSIBLE_SEPARATORS);
    while(token != NULL) {
        strcpy(array_with_args[index_array_args++], token);
        token = strtok(NULL, POSSIBLE_SEPARATORS);
    }

    *length_of_array_with_args = index_array_args;
    return 0;
}

int commandExecutionProcess(int count_of_commands, char** arrayOfCommands) {
    for (int i = 0; i < count_of_commands; ++i) {
        char* command = arrayOfCommands[i];

        int length_of_array_with_args = 0;
        char** array_with_args = (char**)malloc(MAX_NUM_OF_ARGS * sizeof(char*));
        for(int j = 0; j < MAX_NUM_OF_ARGS; ++j) {
            array_with_args[j] = (char*) malloc(MAX_COMMAND_LENGTH * sizeof(char));
        }

        if(splitCommand(command, &length_of_array_with_args, array_with_args)) {
            exit(-1);
        }

        if (!strcmp(array_with_args[0], "echo")) {
            echo(length_of_array_with_args, array_with_args);
            free_string_array(array_with_args, length_of_array_with_args);
        } else if (!strcmp(array_with_args[0], "retcode")) {
            retcode(length_of_array_with_args, array_with_args);
            free_string_array(array_with_args, length_of_array_with_args);
        } else {
            printf("Unknown command\n");
            free_string_array(array_with_args, length_of_array_with_args);
            return UNKNOWN_COMMAND;
        }

    }
    free_string_array(arrayOfCommands, count_of_commands);
    return 0;
}

int commandLineProcess() {
    char *commands = (char*) malloc(MAX_COMMAND_LENGTH * sizeof(char));
    char** arrayOfCommands = (char**)malloc(MAX_NUM_OF_COMMANDS * sizeof(char*));
    for(int i = 0; i < MAX_NUM_OF_COMMANDS; ++i) {
        arrayOfCommands[i] = (char*) malloc(MAX_COMMAND_LENGTH * sizeof(char));
    }
    int length_of_array_commands = 0;

    while(!feof(stdin)) {
        fgets(commands, MAX_COMMAND_LENGTH, stdin);

        char *token;
        token = strtok(commands, SEPARATOR_FOR_THE_COMMANDS);

        while(token != NULL) {
            strcpy(arrayOfCommands[length_of_array_commands++], token);
            token = strtok(NULL, SEPARATOR_FOR_THE_COMMANDS);
        }
    }
    free(commands);

    return commandExecutionProcess(length_of_array_commands, arrayOfCommands);
}


int main(int argc, char *argv[]) {
    return commandLineProcess();
}
