
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_NUM_OF_COMMANDS 64
#define MAX_NUM_OF_ARGS 128
#define MAX_STRING_LENGTH 512
#define SPECIAL_DELIMETER ";"
#define POSSIBLE_DELIMETERS " \t\r\n\v\f"

#define ARRAY_SIZE(x)  (sizeof(x) / sizeof((x)[0]))

int last_echo_return_code;

char* builtin_cmds_str[] =  {
  "echo",
  "retcode"
};

int echo(int argc, char *argv[]) {
	for (int i = 1; i < argc; ++i) {
		printf("%s%c", argv[i], i == argc - 1 ? '\n' : ' ');
	}
	return argc - 1;
}

int retcode(int argc, char *argv[]) {
}

void parse_step_2(char** not_formatted_cmds, int cmd_arg_counter) {
  char formatted_cmds[cmd_arg_counter][MAX_NUM_OF_ARGS][MAX_STRING_LENGTH];
  int number_of_args[MAX_NUM_OF_ARGS];

  for(int i = 0; i < cmd_arg_counter; i++) {
    int counter = 0;
    char *token;
    token = strtok(not_formatted_cmds[i], POSSIBLE_DELIMETERS);
    while(token != NULL) {
      strcpy(formatted_cmds[i][counter++], token);
      token = strtok(NULL, POSSIBLE_DELIMETERS);
    }

    number_of_args[i] = counter;
  }

  for(int i = 0; i < MAX_NUM_OF_COMMANDS; i++) free(not_formatted_cmds[i]);
	free(not_formatted_cmds);

  for(int i = 0; i < cmd_arg_counter; i++) {
    int builtin_cmds_size = ARRAY_SIZE(builtin_cmds_str);
    for(int j = 0; j < builtin_cmds_size; j++) {
      if(!strcmp(builtin_cmds_str[j], formatted_cmds[i][0])) {
          int array_size = number_of_args[i];
          char str[MAX_STRING_LENGTH];
          if(!strcmp(builtin_cmds_str[j], "echo")) {
            last_echo_return_code = array_size - 1;
            for(int k = 1; k < array_size; k++) {
              printf("%s ", formatted_cmds[i][k]);
            }

						//echo(array_size, formatted_cmds[i]);

            printf("\n"); // For seeing result properly
          }

          if(!strcmp(builtin_cmds_str[j], "retcode")) { // A little weird thing
            printf("%d\n", last_echo_return_code);
          }
      }
    }
  }

}

void parse_step_1() { // Separating commands+args when ";"
  char *str = (char*) malloc(MAX_STRING_LENGTH * sizeof(char)); // Input String
  char **not_formatted_cmds = (char**) malloc(MAX_NUM_OF_COMMANDS * sizeof(char*)); // Not-Formated commands array
  for(int i = 0; i < MAX_NUM_OF_COMMANDS; i++) {
    not_formatted_cmds[i] = (char*) malloc(MAX_STRING_LENGTH * sizeof(char));
  }

  int cmd_arg_counter = 0; // Used for counting number of pairs (command, args)
	while(!feof(stdin)) {
    fgets(str, MAX_STRING_LENGTH, stdin);

    char *token;
    token = strtok(str, SPECIAL_DELIMETER);

    while(token != NULL) {
        strcpy(not_formatted_cmds[cmd_arg_counter++], token);
        token = strtok(NULL, SPECIAL_DELIMETER);
    }
  }

  free(str);

  parse_step_2(not_formatted_cmds, cmd_arg_counter);
}

int main(int argc, char *argv[]) {
	parse_step_1();
	return 0;
}
