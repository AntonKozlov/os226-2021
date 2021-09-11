#include <stdio.h>
#include <string.h>
#include "cvector.h"
#define MAX_INPUT_STRING_LEN 100

int RETURN_CODE;

int echo(int argc, char *argv[]) {
    for (int i = 1; i < argc; ++i) {
        printf("%s%c", argv[i], i == argc - 1 ? '\n' : ' ');
    }
    return argc - 1;
}

int retcode(int argc, char *argv[]) {
    printf("%d\n", RETURN_CODE);
    return 0;
}

int mismatch_func(int argc, char *argv[]) {
    fprintf(stderr, "%s %s\n", "Unknown command", argv[0]);
    return -1;
}

int get_function(cvector *vector) {
    char *command = cvector_get(vector, 0);
    int size = cvector_size(vector);
    if (!strcmp(command, "echo")) {
        return echo(size, (char **)vector->data);
    } else if (!strcmp(command, "retcode")) {
        return retcode(size, (char **)vector->data);
    } else {
        return mismatch_func(size, (char **)vector->data);
    }
}

void parse_line(char *input_string, char *delim, cvector *vector) {
    char *pch = strtok(input_string, delim);

    while (pch != NULL) {
        cvector_push(vector, (void *)pch);
        pch = strtok(NULL, delim);
    }
}

void execute_cmd_commands() {
    char input_line[MAX_INPUT_STRING_LEN];
    while(1) {
        fgets(input_line, MAX_INPUT_STRING_LEN, stdin);
        if (!feof(stdin)) {
            printf( "No eof\n");
            if (strcmp(input_line, "") == 0 || strcmp(input_line, "\n") == 0) {
                continue;
            }
            cvector v1;
            cvector_init(&v1, MAX_INPUT_STRING_LEN * sizeof(char));
            parse_line(input_line, ";", &v1);
            for (int i = 0; i < cvector_size(&v1); ++i) {
                char *elem = cvector_get(&v1, i);
                cvector v2;
                cvector_init(&v2, MAX_INPUT_STRING_LEN * sizeof(char));
                parse_line(elem, " \n", &v2);
                if (cvector_size(&v2) == 0) {
                    continue;
                }
                RETURN_CODE = get_function(&v2);
                cvector_clear(&v2);
            }
            cvector_clear(&v1);
        } else {
            printf("EOF\n");
            return;
        }
    }
}

int main() {
    execute_cmd_commands();
    return 0;
}