#include <cstring>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

using namespace std;

typedef struct {
  fstream file;
} filesystem_t;

typedef struct {
  string cmd;
  vector<string> args;
} cmd_t;


filesystem_t fs;


void print_cmd(cmd_t command) {
  cout << endl << "cmd> " <<  command.cmd << endl;

  for (auto it = command.args.begin(); it != command.args.end(); it++) {
    cout << "> " << *it << endl;
  }
  cout << endl;
}


/**
 * Monta o sistema de arquivos a interpreta seus dados, permitindo que seu conteúdo
 * continue a ser manipulado.
 */
void mount(cmd_t command) {
  fs.file.open(command.args[0], ios::in | ios::out | ios::app);

  if (!fs.file.is_open()) {
    cout << "Não foi possível abrir " << command.args[0] << endl;
    exit(1);
  }

  // aqui deve ser feito o parsing do sistema de arquivos
}


/**
 * Salva o estado atual do sistema de arquivos, fechando-o.
 */
void unmount(cmd_t command) {
  // aqui precisa copiar para o disco o que estiver na abstracao

  fs.file.close();
}


/**
 * Recebe uma string contendo uma linha de comando e retorna uma abstração que separa o
 * comando dos seus respectivos argumentos.
 */
cmd_t parse_line(string line) {
  char* token;
  char c_str[line.length() + 1];
  cmd_t command;

  /**
   * Obtém o primeiro token, aka, o comando a ser executado.
   */
  strcpy(c_str, line.c_str());
  token = strtok(c_str, " ");
  command.cmd = token;

  /**
   * Em seguida inicia o processo de parsing dos argumentos para o comando. Note
   * que strtok é chamado de novo de modo que `token` armazene o primeiro argumento
   * de fato, em vez do comando em si.
   */
  token = strtok(NULL, " ");
  while (token != NULL) {
    command.args.push_back(token);
    token = strtok(NULL, " ");
  }

  return command;
}


/**
 * Exibe o prompt e faz o parse do input recebido.
 */
cmd_t prompt_command() {
  string line;

  printf("[ep3]: ");
  getline(cin, line);

  return parse_line(line);
}


void execute(cmd_t command) {
  if (command.cmd == "mount") {
    mount(command);
  } else if (command.cmd == "unmount") {
    unmount(command);
  }
}


int main() {
  // filesystem_t fs;
  cmd_t command;

  while (1) {
    command = prompt_command();

    if (command.cmd == "sair") break;
    execute(command);
  }

  return 1;
}