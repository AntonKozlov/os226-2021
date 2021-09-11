#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_NUM_OF_CMDS 64
#define MAX_NUM_OF_ARGS 128
#define MAX_STR_LEN 512
#define INT_TO_STR_ARR_SIZE 64
#define CMD_SEPARATOR ";"
#define DELIMITERS " \t\r\n\v\f"

char* builtin_cmds[] = {
  "echo",
  "retcode"
};

int echo(int argc, char* argv[]) { // Changed 1 to 2 because of "formatted_data" structure
	for (int i = 2; i < argc; i++) {
		printf("%s%c", argv[i], i == argc - 1 ? '\n' : ' ');
	}

	return argc - 2;
}

int retcode(int argc, char* argv[]) {
  printf("%d\n", argc - 2);
}

char* allocate_char_array(size_t size1) {
  char* array = (char*) malloc(size1 * sizeof(char));

  return array;
}

int* allocate_int_array(size_t size1) {
  int* array = (int*) malloc(size1 * sizeof(int));

  return array;
}

char** allocate_double_string_array(size_t size1, size_t size2) { // From greater to lower
  char** array = (char**) malloc(size1 * sizeof(char*));
  for(size_t i = 0; i < size1; i++) {
    array[i] = (char*) malloc(size2 * sizeof(char));
  }

  return array;
}

char*** allocate_triple_string_array(size_t size1, size_t size2, size_t size3) {
  char*** array = (char***) malloc(size1 * sizeof(char**));
  for(size_t i = 0; i < size1; i++) {
    array[i]  = (char**) malloc(size2 * sizeof(char*));
    for(size_t j = 0; j < size2; j++) {
      array[i][j]  = (char*) malloc(size3 * sizeof(char));
    }
  }

  return array;
}

void free_char_array(char* array) {
  free(array);
}

void free_int_array(int* array) {
  free(array);
}

void free_double_string_array(size_t size1, char** array) {
  for(size_t i = 0; i < size1; i++) {
    free(array[i]);
  }

  free(array);
}

void free_triple_string_array(size_t size1, size_t size2, char*** array) {
  for(size_t i = 0; i < size1; i++) {
    for(size_t j = 0; j < size2; j++) {
      free(array[i][j]);
    }

    free(array[i]);
  }

  free(array);
}

void parse_commands(int line_counter, char*** formatted_data) {
  for(int i = 0; i < line_counter; i++) {
    if(!strcmp(formatted_data[i][1], builtin_cmds[0])) {
      echo(atoi(formatted_data[i][0]), formatted_data[i]);
    }

    if(!strcmp(formatted_data[i][1], builtin_cmds[1])) {
      if(i > 0) {
        retcode(atoi(formatted_data[i - 1][0]), formatted_data[i]);
      }

      else { // If the first cmd would be "retcode", return 0
        retcode(2, NULL);
      }
    }
  }
}

int process_input() {
  char* input_str = allocate_char_array(MAX_STR_LEN);
  if(input_str == NULL) return 1;

  char** not_formatted_data = allocate_double_string_array(MAX_NUM_OF_CMDS, MAX_STR_LEN);
  if(not_formatted_data == NULL) return 1;

  int line_counter = 0;

	while(!feof(stdin)) {
    fgets(input_str, MAX_STR_LEN, stdin);

    char *token;
    token = strtok(input_str, CMD_SEPARATOR);

    while(token != NULL) {
        strcpy(not_formatted_data[line_counter++], token);
        token = strtok(NULL, CMD_SEPARATOR);
    }
  }

  free_char_array(input_str);

  char* int_to_string_array = allocate_char_array(INT_TO_STR_ARR_SIZE);
  if(int_to_string_array == NULL) return 1;

  char*** formatted_data = allocate_triple_string_array(line_counter, MAX_NUM_OF_ARGS, MAX_STR_LEN);
  if(formatted_data == NULL) return 1;

  int offset = 0; // Used for eliminating empty lines
  for(int i = 0; i < line_counter; i++) {
    int counter = 1;

    char *token;
    token = strtok(not_formatted_data[i], DELIMITERS);

    if(token != NULL) {
      while(token != NULL) {
        strcpy(formatted_data[i - offset][counter++], token);
        token = strtok(NULL, DELIMITERS);
      }

      sprintf(int_to_string_array, "%d", counter);
      strcpy(formatted_data[i - offset][0], int_to_string_array);
    }

    else {
      offset++;
    }
  }

  line_counter -= offset;

  free_char_array(int_to_string_array);
  free_double_string_array(MAX_NUM_OF_CMDS, not_formatted_data);

  parse_commands(line_counter, formatted_data);

  free_triple_string_array(line_counter, MAX_NUM_OF_ARGS, formatted_data);

  return 0;
}

int execute_command_parser() {
  return process_input();
}

int main(int argc, char *argv[]) {
  if(execute_command_parser()) return execute_command_parser();

	return 0;
}
