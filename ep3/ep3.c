#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LINE_LEN 200

typedef struct {
  char* cmd;
  char** args;
  int args_len;
} cmd_t;


void print_cmd(cmd_t* command) {
  printf("cmd> %s\n", command->cmd);

  for (int i = 0; i < command->args_len; i++) {
    printf("> %s\n", command->args[i]);
  }
}


cmd_t* parse_line(char* line) {
  char* token;
  cmd_t* command;

  command = malloc(sizeof(cmd_t));
  token = strtok(line, " ");

  while (token != NULL) {
    token = strtok(line, NULL);
  }

  return command;
}

cmd_t* prompt_command() {
  char *line;

  printf("[ep3]: ");
  fgets(line, LINE_LEN, stdin);

  return parse_line(line);
}


void execute(cmd_t *command) {
  printf("> %s\n", command->cmd);
}


int main() {
  cmd_t* command;

  while (1) {
    command = prompt_command();

    if (strcmp(command->cmd, "sair") == 0) break;
    execute(command);
  }

  return 1;
}