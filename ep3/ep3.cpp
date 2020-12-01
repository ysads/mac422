#include <cstring>
#include <ctime>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#define BLOCK_SIZE 4000
// #define MAX_FS_SIZE 100000000
#define MAX_FS_SIZE 64000  // teste
#define MAX_BLOCKS (MAX_FS_SIZE / BLOCK_SIZE)

using namespace std;

/**
 * Representa os metadados de um único arquivo dentro do sistema de arquivos.
 * Note que, apesar de não haver um limite explícito para o tamanho de `name`,
 * consideremos como 255 caracters tal limite.
 */
typedef struct {
  string name;
  size_t size;
  time_t created;
  time_t last_access;
  time_t last_modified;
} vfile_t;

/**
 * Representa um diretório dentro do sistema de arquivos.
 */
typedef struct {
  string name;
  vector<vfile_t> children;
} vdir_t;

/**
 * Representa o sistema de arquivos propriamente dito, incluindo tabela FAT,
 * o bitmap representando os blocos livres/usados e o arquivo no sistema físico
 * onde nossa abstração vai ser armazenada.
 */
typedef struct {
  fstream file;
  void* fat[MAX_BLOCKS];
  int bitmap[MAX_BLOCKS];
} filesystem_t;

/**
 * Abstrai um comando para o sistema de arquivo, separando comando built-in
 * dos seus argumentos.
 */
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
 * Diz se o arquivo lido está vazio comparando o número de bytes da atual posição.
 * OBS: depende de a posição atual corresponder ao final do arquivo.
 */
bool is_fs_empty() {
  cout << "pos: " << fs.file.tellg() << endl;
  return fs.file.tellg() <= 0;
}


/**
 * Já que não é possível escrever um bit por vez no arquivo, a única forma de
 * sincronizar o bitmap é condensando vários grupos de 8 bits consecutivos num único
 * byte que os represente. Isso é feito tomando a representação desses 8 bits num
 * inteiro entre 0 e 255.
 */
void write_bitmap_to_fs() {
  int j;
  char encoded_segment;
  string segment;

  cout << "bef seek: " << fs.file.tellp() << endl;
  fs.file.seekp(ios::beg);
  cout << "aft seek: " << fs.file.tellp() << endl;

  for (int i = 0; i < MAX_BLOCKS; i += 8) {
    segment = "";

    /**
     * Agrega os 8 bits numa string única
     */
    for (int j = i; j < (i+8); j++) {
      segment.append(to_string(fs.bitmap[j]));
    }

    /**
     * O inteiro equivalente à string gerada acima é obtido com a conversão abaixo,
     * sendo escrito no arquivo.
     */
    encoded_segment = stoi(segment, 0, 2);

    fs.file << encoded_segment;
  }
}


/**
 * Inicializa um filesystem vazio no arquivo apontado em fs. Isso inclui escrever lá
 * um bitmap vazio, uma quase–vazia tabela fat e os metadados do diretório /
 */
void init_empty_fs() {
  // marca como 0 os primeiros blocos ocupados do disco – bitmap, fat e metadata de /
  for (int i = 0; i < MAX_BLOCKS; i++) {
    fs.bitmap[i] = 1;
  }

  write_bitmap_to_fs();
  // inicializa o diretório
}


/**
 * Monta o sistema de arquivos a interpreta seus dados, permitindo que seu conteúdo
 * continue a ser manipulado.
 */
void mount(cmd_t command) {
  /**
   * Garante que o filesystem simulado sempre vai existir por, inicialmente, tentar
   * abri-lo em modo append – o que o acaba criando, caso ele não exista.
   */
  fs.file.open(command.args[0], ios::out | ios::app);
  fs.file.close();
  fs.file.open(command.args[0], ios::in | ios::out | ios::ate);

  if (!fs.file.is_open()) {
    cout << "Não foi possível abrir " << command.args[0] << endl;
    exit(1);
  }

  if (is_fs_empty()) {
    cout << "Vazio!" << endl;
    init_empty_fs();
    // aqui deve ser feito o parsing do sistema de arquivos
  } else {
    cout << "num é vazi" << endl;
  }
}


/**
 * Salva o estado atual do sistema de arquivos, fechando-o.
 */
void unmount(cmd_t command) {
  write_bitmap_to_fs();
  // aqui precisa copiar para o disco a fat atualizada

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
  cmd_t command;

  while (1) {
    command = prompt_command();

    if (command.cmd == "sai") break;
    execute(command);
  }

  return 1;
}