#include <cstring>
#include <ctime>
#include <cinttypes>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

/**
 * Capacidade máxima do filesystem.
 */
#define MAX_FS_SIZE 100000000

/**
 * Número máximo de blocos presentes no filesystem.
 */
#define MAX_BLOCKS (MAX_FS_SIZE / BLOCK_SIZE)

/**
 * Número de bytes ocupado por um único bloco no filesystem.
 */
#define BLOCK_SIZE 4000

/**
 * Número de bytes ocupados pelo bitmap.
 */
#define BITMAP_SIZE (7 * BLOCK_SIZE)

/**
 * Número de bytes ocupados pela tabela FAT.
 */
#define FAT_SIZE (MAX_BLOCKS * sizeof(int))

/**
 * Posição dentro do FS a partir da qual podem ser escritos arquivos e diretórios
 */
#define INITIAL_OFFSET (BITMAP_SIZE + FAT_SIZE + BLOCK_SIZE)

/**
 * Representa o primeiro bloco disponível para uso no sistema.
 */
#define INITIAL_BLOCK (INITIAL_OFFSET / BLOCK_SIZE)

/**
 * Tamanho máximo que um nome de arquivo pode ter.
 */
#define FILENAME_SIZE 120

/**
 * O tamanho máximo dos metadados de um arquivo.
 */
#define CHILD_SIZE 160

/**
 * O número máximo de arquivos filhos em um diretório.
 */
#define MAX_CHILDREN_PER_DIR (BLOCK_SIZE / CHILD_SIZE)


#define debug(k, v) cout << k << ": " << v << endl;
#define fail(msg) cerr << "=> ERRO: " << msg << endl;

using namespace std;

typedef struct {
  string name;
  time_t created;
  time_t last_access;
  time_t last_modified;
  size_t size;
  int is_dir;
  int head;
} vattr_t;

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
  int head;
} vfile_t;

/**
 * Representa um diretório dentro do sistema de arquivos.
 */
typedef struct {
  string name;
  vector<vattr_t> children;
} vdir_t;

/**
 * Representa o sistema de arquivos propriamente dito, incluindo tabela FAT,
 * o bitmap representando os blocos livres/usados e o arquivo no sistema físico
 * onde nossa abstração vai ser armazenada.
 */
typedef struct {
  fstream file;
  int fat[MAX_BLOCKS];
  char bitmap[MAX_BLOCKS];
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


void print_attr(vattr_t attrs) {
  printf("------------------\n");
  printf("%s (%zu bytes)\n",  attrs.name.c_str(), attrs.size);
  printf("|> #%d\n", attrs.head);
  printf("|> create: %ld\n", attrs.created);
  printf("|> access: %ld\n", attrs.last_access);
  printf("|> modify: %ld\n", attrs.last_modified);
  printf("|> null? %d\n", (attrs.created == NULL));
  printf("|> dir? %d\n", (attrs.size == -1));
}

void print_dir(vdir_t dir) {
  if (dir.children.size() == MAX_CHILDREN_PER_DIR) {
    printf("----------------\n");
    printf("|> empty dir!\n");
  } else {
    for (auto child: dir.children) {
      print_attr(child);
    }
  }
}


/**
 * Diz se o arquivo lido está vazio comparando o número de bytes da atual posição.
 * OBS: depende de a posição atual corresponder ao final do arquivo.
 */
bool is_fs_empty() {
  return fs.file.tellg() <= 0;
}


/**
 * Já que não é possível escrever um bit por vez no arquivo, a gente converte o bitmap
 * numa sequência gigante de 0's e 1's a serem inseridos de forma sequencial no arquivo.
 * Observe que sobra espaço no último bloco (3kb precisamente), que são ocupados com
 * caracteres aleatórios de modo a garantir que todo o espaço do bloco seja ocupado.
 */
void write_bitmap_to_fs() {
  int padding;
  char padded_bitmap[BITMAP_SIZE];

  padding = BITMAP_SIZE - MAX_BLOCKS;

  for (int i = 0; i < MAX_BLOCKS; i++) {
    padded_bitmap[i] = fs.bitmap[i];
  }
  for (int i = 0; i < padding; i++) {
    padded_bitmap[MAX_BLOCKS+i] = '_';
  }

  fs.file.seekp(ios::beg);
  fs.file.write((char*) padded_bitmap, BITMAP_SIZE);
}


/**
 * Escreve a tabela FAT no filesystem serializando-a como uma sequência de chars.
 * Note que o tamanho dela (100kb) é comportado de forma precisa dentro dos blocos
 * de 4kb utilizados – totalizando 25 blocos.
 */
void write_fat_to_fs() {
  fs.file.seekp(BITMAP_SIZE, ios::beg);
  fs.file.write((char*) fs.fat, MAX_BLOCKS * sizeof(int));
}


/**
 * Escreve os metadados de um diretório na posição dada.
 */
void write_dir_to_fs(int position, vdir_t content) {
  fs.file.seekp(position, ios::beg);

  // debug("nous some", position);
  debug("block", position / BLOCK_SIZE);
  debug("will write children", content.children.size());

  for (auto child : content.children) {
    debug("escrevendo", child.name);
    fs.file.write((char*) &child.created, sizeof(time_t));
    fs.file.write((char*) &child.last_access, sizeof(time_t));
    fs.file.write((char*) &child.last_modified, sizeof(time_t));
    fs.file.write(reinterpret_cast<const char*>(&child.size), sizeof(size_t));
    fs.file.write(reinterpret_cast<const char*>(&child.head), sizeof(int));
    fs.file.write(reinterpret_cast<const char*>(&child.is_dir), sizeof(int));
    fs.file.write(child.name.c_str(), FILENAME_SIZE);
  }

  /**
   * Um metadado de controle, com created nulo, é adicionado após o último filho
   * escrito no bloco, de modo a registrar o "fim" da lista de filhos. Isso não é
   * necessário quando um diretório possui o número máximo de filhos possível.
   */
  if (content.children.size() != MAX_CHILDREN_PER_DIR) {
    time_t null_time = NULL;
    fs.file.write((char*) &null_time, sizeof(time_t));
  }
  // fs.file.flush();
}


/**
 * Inicializa os metadados do diretório '/', armazenando-os no bloco
 * imediatamente após o último bloco ocupado pela tabela FAT.
 */
void init_empty_root() {
  vdir_t root_dir, root_dir_parent;
  vattr_t root_attrs;
  time_t now;
  int root_pos;

  now = time(nullptr);
  root_pos = (BITMAP_SIZE + FAT_SIZE) / BLOCK_SIZE;

  /**
   * Define os atributos do diretório '/'
   */
  root_attrs.created = now;
  root_attrs.last_access = now;
  root_attrs.last_modified = now;
  root_attrs.size = 0;
  root_attrs.head = root_pos + 1;
  root_attrs.is_dir = 1;
  root_attrs.name = "/";

  /**
   * Adiciona '/' como filho do pseudo-diretório inicial.
   */
  root_dir_parent.children.push_back(root_attrs);

  fs.bitmap[root_pos] = '0';
  fs.fat[root_pos] = -1;

  fs.bitmap[root_pos+1] = '0';
  fs.fat[root_pos+1] = -1;

  debug("usando bloco", root_pos);
  debug("usando bloco", root_pos+1);

  write_dir_to_fs(root_pos * BLOCK_SIZE, root_dir_parent);
  write_dir_to_fs((root_pos + 1) * BLOCK_SIZE, root_dir);
}


/**
 * Inicializa um filesystem vazio no arquivo apontado em fs. Isso inclui escrever lá
 * um bitmap vazio, uma quase–vazia tabela fat e os metadados do diretório /
 */
void init_empty_fs() {
  for (int i = 0; i < MAX_BLOCKS; i++) {
    fs.bitmap[i] = '1';
    fs.fat[i] = 0;
  }

  write_bitmap_to_fs();
  write_fat_to_fs();
  init_empty_root();
}


/**
 * Reads an already initialized filesystem, parsing its bitmap FAT table.
 */
void parse_fs() {
  fs.file.seekg(ios::beg);
  fs.file.read((char*) fs.bitmap, MAX_BLOCKS);

  fs.file.seekg(BITMAP_SIZE, ios::beg);
  fs.file.read((char*) fs.fat, MAX_BLOCKS * sizeof(int));
}


/**
 * Interpreta o diretório contido na posição especificada retornando-o.
 */
vdir_t parse_dir(int position) {
  vdir_t dir;
  char name[FILENAME_SIZE];
  int block;

  /**
   * Verifica se esse é um bloco livre. Se for, não há o que parsear.
   */
  block = position / BLOCK_SIZE;
  if (fs.bitmap[block] == '1') {
    // printf("vazio: bloco %d\n", block);
    return dir;
  }
  // printf("\ndir @ #%d (byte %d)...\n", position);

  fs.file.seekg(ios::beg);
  fs.file.seekg(position);

  do {
    vattr_t attrs;

    fs.file.read((char*) &attrs.created, sizeof(time_t));
    if (attrs.created == NULL) break;

    fs.file.read((char*) &attrs.last_access, sizeof(time_t));
    fs.file.read((char*) &attrs.last_modified, sizeof(time_t));
    fs.file.read(reinterpret_cast<char*>(&attrs.size), sizeof(size_t));
    fs.file.read(reinterpret_cast<char*>(&attrs.head), sizeof(int));
    fs.file.read(reinterpret_cast<char*>(&attrs.is_dir), sizeof(int));
    fs.file.read(name, FILENAME_SIZE);

    attrs.name = name;
    dir.children.push_back(attrs);

    print_attr(attrs);
  } while (dir.children.size() < MAX_CHILDREN_PER_DIR);

  return dir;
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
  fs.file.open(command.args[0], ios::in | ios::out | ios::ate | ios::binary);

  if (!fs.file.is_open()) {
    cout << "Não foi possível abrir " << command.args[0] << endl;
    exit(1);
  }

  if (is_fs_empty()) {
    init_empty_fs();
  } else {
    parse_fs();
  }
}


/**
 * Salva o estado atual do sistema de arquivos, fechando-o.
 */
void unmount(cmd_t command) {
  write_bitmap_to_fs();
  write_fat_to_fs();
  // aqui precisa copiar para o disco a fat atualizada

  fs.file.close();
}


// void rmdir (cmd_t command) {
//   vdir_t* diretorio;

//   /*encontrar o diretorio do command*/

//   for(i=0;diretorio->children[i]!=null;i++){
//     cmd_t comando;
//     comando->cmd = "rm ";
//     comando-> args[0] = diretorio->children[i]->name;
//     cout >> "Removendo subarquivo " >> diretorio->children[i]->name >> endl;
//     remove_file(comando);
//   }

//   for(i=0; diretorio->dir_children[i]!= null; i++){
//     cmd_t comando;
//     comando->cmd = command->cmd;
//     comando->args[0]= diretorio->dir_children[i]->name;
//     cout >> "Removendo subpasta" >> diretorio->dir_children[i]->name >> endl;
//     rmdir(comando);
//   }
// }


// void df(){
//   /*estatisticas*/
// }

// void find_file (cmd_t command){
//   vdir_t* pasta;

//   /*pasta= pasta com o nome command->args[0];*/

//   for(vfile_t* procura:pasta->children){
//     if(procura.find(command->args[1])!=-1){
//       cout << procura->name << endl;;
//     }
//   }

//   for(vdir_t* subpasta:pasta->dir_children){
//     cmd_t comando;
//     comando->cmd=command->cmd;
//     comando->args[0]=subpasta;
//     comando->args[1]=commands->args[1];
//     find_file(subpasta);
//   }
// }


// void print_file (cmd_t command){
//   string line;
//   fstream arquivo;
//   arquivo.open(command->args[0]);
//   if(arquivo.is_open()){
//     while(getline(arquivo, line)){
//       cout << line << endl;
//     }
//     aruivo.close();
//   }
//   else{
//     cout << "Arquivo "<< command->args[0] << " não encontrado\n";
//   }
//   /*abrir arquivo e printar ele*/
// }


/**
 * Busca linearmente em todo o filesystem pelo primeiro block vazio disponível.
 * @throw quando não há espaço disponível.
 */
int find_empty_block() {
  for (int i = INITIAL_BLOCK; i < MAX_BLOCKS; i++) {
    // printf("#%d? %c\n", i, fs.bitmap[i]);
    if (fs.bitmap[i] == '1') {
      return i;
    }
  }
  throw "Não há espaço disponível em disco!";
}


/**
 * Cria um novo arquivo vazio no filesystem, usando o primeiro bloco disponível.
 */
vattr_t init_file(string filename, int is_dir) {
  vattr_t new_file;
  time_t now;
  int head_block;

  now = time(nullptr);
  head_block = find_empty_block();

  new_file.created = now;
  new_file.last_access = now;
  new_file.last_modified = now;
  new_file.name = filename;
  new_file.size = 0;
  new_file.head = head_block;
  new_file.is_dir = is_dir;

  /**
   * Marca como usado, no filesystem, o espaço do arquivo recém-criado.
   */
  fs.bitmap[head_block] = '0';
  fs.fat[head_block] = -1;

  return new_file;
}


/**
 * Adiciona no diretório um novo arquivo vazio, usando o nome passado.
 * @throw quando o diretório já está cheio.
 */
vdir_t insert_file_into_dir(vdir_t dir, string child_name, int is_dir) {
  vattr_t file;

  if (dir.children.size() == MAX_CHILDREN_PER_DIR) {
    throw "Diretório cheio. Não é possível adicionar conteúdo aqui!";
  }

  /**
   * Inicializa um arquivo vazio e o adiciona ao diretório.
   */
  file = init_file(child_name, is_dir);
  dir.children.push_back(file);

  return dir;
}


void mkdir(cmd_t command) {
  string dir_name;
  streampos dir_position;
  vdir_t dir;
  vdir_t new_dir;

  dir_name = command.args[0];
  dir_position = INITIAL_OFFSET;
  dir = parse_dir(dir_position);

  for (int i = 0; i < dir.children.size(); i++) {
    /**
     * Localizamos o arquivo nesse diretório. Atualiza o last_access e escreve
     * o diretório de volta no filesystem.
     */
    if (dir.children[i].name == dir_name) {
      throw ("Não é possível criar diretório. " +  dir_name + " já existe!").c_str();
    }
  }

  new_dir = insert_file_into_dir(dir, dir_name, 1);
  print_dir(new_dir);
  write_dir_to_fs(dir_position, new_dir);
}


void touch(cmd_t command) {
  string searched_file;
  vdir_t dir;
  int dir_position;

  searched_file = command.args[0];
  dir_position = INITIAL_OFFSET;
  dir = parse_dir(dir_position);

  // for (int i = 0; i < dir.children.size(); i++) {
  //   /**
  //    * Localizamos o arquivo nesse diretório. Atualiza o last_access e escreve
  //    * o diretório de volta no filesystem.
  //    */
  //   if (dir.children[i].name == searched_file) {
  //     dir.children[i].last_access = time(nullptr);
  //     write_dir_to_fs(dir_position, dir);
  //     return;
  //   }
  // }

  // printf("parse: [ %lu children ]\n", dir.children.size());

  dir = insert_file_into_dir(dir, searched_file, 0);
  write_dir_to_fs(dir_position, dir);

  // printf("after: [ %lu children ]\n\n", dir.children.size());

  // print_dir(dir);
}


// void remove_file (cmd_t command){
//   vfile_t* arquivo;
//   /*encontrar arquivo e remove-lo pasta em que ele está*/
//   free(arquivo);
// }


// void print_dir (cmd_t command){
//   vdir_t* diretorio;

//   /*encontrar diretorio*/

//   cout << "Arquivos na pasta " << diretorio->name <<":\n";
//   for(i=0;diretorio->children[i]!=null;i++){
//     cout << diretorio->children[i]->name << endl;
//   }

//   cout << endl << "Pastas na pasta " << diretorio->name << ":\n";
//   for(i=0;diretorio->dir_children[i]!=null;i++){
//     cout << diretorio->dir_children[i]->name << endl;
//   }

//   if(diretorio->children.empty() && diretorio->dir_children.empty()){
//     cout << "A pasta " << diretorio->name << " está vazia\n";
//   }
// }


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


void hook(cmd_t command) {
  vdir_t dir;
  char name[FILENAME_SIZE];
  int block = stoi(command.args[0]);
  int pos = block * BLOCK_SIZE;

  fs.file.seekg(ios::beg);
  fs.file.seekg(pos);

  printf("\nhooking into #%d (byte %d)...\n", block, pos);
  // printf("@ %zu byte\n", fs.file.tellg());
  // printf("bitmap: %c – fat: %d\n", fs.bitmap[block], fs.fat[block]);

  do {
    vattr_t attrs;

    fs.file.read((char*) &attrs.created, sizeof(time_t));
    if (attrs.created == NULL) break;

    fs.file.read((char*) &attrs.last_access, sizeof(time_t));
    fs.file.read((char*) &attrs.last_modified, sizeof(time_t));
    fs.file.read(reinterpret_cast<char*>(&attrs.size), sizeof(size_t));
    fs.file.read(reinterpret_cast<char*>(&attrs.head), sizeof(int));
    fs.file.read(reinterpret_cast<char*>(&attrs.is_dir), sizeof(int));
    fs.file.read(name, FILENAME_SIZE);
    attrs.name = name;

    dir.children.push_back(attrs);

  } while (dir.children.size() < MAX_CHILDREN_PER_DIR);

  print_dir(dir);

  printf("\n[ %lu children read ]\n\n", dir.children.size());
}


void execute(cmd_t command) {
  try {
    if (command.cmd == "mount") {
      mount(command);
    } else if (command.cmd == "unmount") {
      unmount(command);
    } else if (command.cmd == "touch") {
      touch(command);
    } else if (command.cmd == "mkdir") {
      mkdir(command);
    } else if (command.cmd == "hook") {
      hook(command);
    }
  } catch (const char* msg) {
    fail(msg);
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
  return 1;
}
