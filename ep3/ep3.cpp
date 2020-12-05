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
  vector<vdir_t> dir_children;
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

time_t start;
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
 * Já que não é possível escrever um bit por vez no arquivo, a gente converte o bitmap
 * na sua representação decimal. Como em geral um inteiro usa 4 bytes para ser representado,
 * são escritos zeros à esquerda do inteiro de modo que ele ocupe o primeiro todo o primeiro
 * block, ie 4kb.
 */
void write_bitmap_to_fs() {
  int padding;
  string binary;

  fs.file.seekp(ios::beg);

  binary = "";
  padding = BLOCK_SIZE - sizeof(int);

  for (int i = 0; i < MAX_BLOCKS; i++) {
    binary.append(to_string(fs.bitmap[i]));
  }
  for (int i = 0; i < padding; i++) {
    fs.file << "0";
  }
  fs.file << stoi(binary, 0, 2);
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


void mkdir (cmd_t command) {
  vdir_t* diretorio;

  diretorio=malloc(sizeof(vdir_t*));

  diretorio->name =command->argv[0];
  for(i=1;command->args[i]!=null;i++){
    diretorio->name+= " ";
    diretorio->name+= command->args[i];
  }
  /*colocar no ARQUIVO */

}


void rmdir (cmd_t command) {
  vdir_t* diretorio;

  /*encontrar o diretorio do command*/

  for(i=0;diretorio->children[i]!=null;i++){
    cmd_t comando;
    comando->cmd = "rm ";
    comando-> args[0] = diretorio->children[i]->name;
    cout >> "Removendo subarquivo " >> diretorio->children[i]->name >> endl;
    remove_file(comando);
  }

  for(i=0; diretorio->dir_children[i]!= null; i++){
    cmd_t comando;
    comando->cmd = command->cmd;
    comando->args[0]= diretorio->dir_children[i]->name;
    cout >> "Removendo subpasta" >> diretorio->dir_children[i]->name >> endl;
    rmdir(comando);
  }
}


void df(){
  /*estatisticas*/
}

void find_file (cmd_t command){
  vdir_t* pasta;

  /*pasta= pasta com o nome command->args[0];*/

  for(vfile_t* procura:pasta->children){
    if(procura.find(command->args[1])!=-1){
      cout << procura->name << endl;;
    }
  }

  for(vdir_t* subpasta:pasta->dir_children){
    cmd_t comando;
    comando->cmd=command->cmd;
    comando->args[0]=subpasta;
    comando->args[1]=commands->args[1];
    find_file(subpasta);
  }


}


void print_file (cmd_t command){
  string line;
  fstream arquivo;
  arquivo.open(command->args[0]);
  if(arquivo.is_open()){
    while(getline(arquivo, line)){
      cout << line << endl;
    }
    aruivo.close();
  }
  else{
    cout << "Arquivo "<< command->args[0] << " não encontrado\n";
  }
  /*abrir arquivo e printar ele*/
}


void touch_file (cmd_t command){
  vfile_t* arquivo;

  /*procurar arquivo*/

  if(/*achou arquivo na procura*/) time(&arquivo->last_access);
  else{
    arquivo=malloc(sizeof(vfile_t));
    arquivo->name = command->args[0];
    for(i=1;arquivo->args[i]!=null;i++){
      arquivo->name+= " ";
      arquivo->name+= command->args[i];
    }

    arquivo->size=?
    time(&arquivo->created)
  }

}


void remove_file (cmd_t command){
  vfile_t* arquivo;
  /*encontrar arquivo e remove-lo pasta em que ele está*/
  free(arquivo);
}


void print_dir (cmd_t command){
  vdir_t* diretorio;

  /*encontrar diretorio*/

  cout << "Arquivos na pasta " << diretorio->name <<":\n";
  for(i=0;diretorio->children[i]!=null;i++){
    cout << diretorio->children[i]->name << endl;
  }

  cout << endl << "Pastas na pasta " << diretorio->name << ":\n";
  for(i=0;diretorio->dir_children[i]!=null;i++){
    cout << diretorio->dir_children[i]->name << endl;
  }

  if(diretorio->children.empty() && diretorio->dir_children.empty()){
    cout << "A pasta " << diretorio->name << " está vazia\n";
  }
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
  switch(command.cmd){
    case "mount":
      mount(command);
      break;
    case "unmount":
      unmount(command);
      break;
    case "cp":
      cp(command);
      break;
    case "mkdir":
      create_dir(command);
      break;
    case "rmdir":
      delete_dir(command);
      break;
    case "cat":
      print_file(command);
      break;
    case "touch":
      touch_file(command);
      break;
    case "rm":
      remove_file(command);
      break;
    case "ls":
      print_dir(command);
      break;
    case "find":
      find_file(command);
      break;
    case "df":
      df();
      break;
  }
  if (command.cmd == "mount") {
  } else if (command.cmd == "unmount") {
  }
}


int main() {
  cmd_t command;

  time(&start);

  while (1) {
    command = prompt_command();

    if (command.cmd == "sai") break;
    execute(command);
  }
  salva_arquivos();
  return 1;
}
