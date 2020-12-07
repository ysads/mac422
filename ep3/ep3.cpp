#include <cstring>
#include <ctime>
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
 * Número de blocos ocupados pelo bitmap.
 */
#define BITMAP_SIZE 7

/**
 * Número de blocos ocupados pela tabela FAT.
 */
#define FAT_SIZE 32

/**
 * Representa o primeiro bloco disponível para uso no sistema.
 */
#define INITIAL_BLOCK (BITMAP_SIZE + FAT_SIZE)

/**
 * Tamanho máximo que um nome de arquivo pode ter.
 */
#define NAME_SIZE 119

/**
 * O tamanho máximo dos metadados de um arquivo.
 */
#define CHILD_SIZE 160

/**
 * O número máximo de arquivos filhos em um diretório.
 */
#define MAX_CHILDREN_PER_DIR (BLOCK_SIZE / CHILD_SIZE)


using namespace std;

#define debug(k, v) cerr << k << ": " << v << endl;
#define fail(msg) cerr << "=> ERRO: " << msg << endl;


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
} vfile_t;

/**
 * Representa um diretório dentro do sistema de arquivos.
 */
typedef struct vdir {
  int head;
  string name;
  time_t created;
  time_t last_access;
  time_t last_modified;
  vector<vfile_t> file_children;
  vector<struct vdir> dir_children;
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
  int bitmap[MAX_BLOCKS];
  string blocks[MAX_BLOCKS];
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
 * Inicializa o bitmap dentro do blocos que o armazenariam no filesystem simulado. Isso torna
 * mais fácil a posterior serialização desses dados para o arquivo real.
 */
void write_bitmap_to_blocks() {
  int i, j, k;
  string serialized;

  i = j = k = 0;
  serialized = "";

  while (i < MAX_BLOCKS) {
    if (j == BLOCK_SIZE) {
      fs.blocks[k] = serialized;
      serialized = "";
      j = 0;
      k++;
    }
    serialized.append(to_string(fs.bitmap[i]));
    i++;
    j++;
  }

  /**
   * Completa o bloco restante com um caracter arbitrário até que atinja BLOCK_SIZE bytes.
   */
  for (j = j; j < BLOCK_SIZE; j++) {
    serialized.append("@");
  }
  fs.blocks[k] = serialized;
}


/**
 * Inicializa a tabela FAT dentro do blocos que o armazenariam no filesystem simulado.
 * Isso torna mais fácil a posterior serialização desses dados para o arquivo real.
 */
void write_fat_to_blocks() {
  int i, j, k;
  char number_repr[6];
  string serialized;

  i = j = 0;
  k = BITMAP_SIZE;

  /**
   * Aqui opta-se por serializar todos os endereços com largura fixa de 5 caracteres. Isso
   * facilita durante o parsing.
   */
  while (i < MAX_BLOCKS) {
    if (j == BLOCK_SIZE) {
      fs.blocks[k] = serialized;
      serialized = "";
      j = 0;
      k++;
    }
    sprintf(number_repr, "%05d", fs.fat[i]);
    serialized.append(string(number_repr));
    i++;
    j += 5;
  }

  /**
   * Completa o bloco restante com um caracter arbitrário até que atinja BLOCK_SIZE bytes.
   */
  for (j = j; j < BLOCK_SIZE; j++) {
    serialized.append("@");
  }
  fs.blocks[k] = serialized;
}


/**
 * Serializa os blocos dentro do arquivo real que representa o filesystem.
 */
void write_blocks_to_fs() {
  fs.file.seekp(ios::beg);

  for (int i = 0; i < MAX_BLOCKS; i++) {
    fs.file << fs.blocks[i];
  }
}


/**
 * Busca linearmente em todo o filesystem pelo primeiro block vazio disponível.
 * @throw quando não há espaço disponível.
 */
int find_empty_block() {
  for (int i = INITIAL_BLOCK; i < MAX_BLOCKS; i++) {
    if (fs.bitmap[i] == '1') {
      return i;
    }
  }
  throw "Não há espaço disponível em disco!";
}


/**
 * Inicializa um diretório vazio a partir do primeiro bloco disponível.
 */
vdir_t new_dir(string name) {
  vdir_t dir;
  time_t now;
  int head_block;

  now = time(nullptr);
  head_block = find_empty_block();

  dir.created = now;
  dir.last_access = now;
  dir.last_modified = now;
  dir.name = name;
  dir.head = head_block;

  /**
   * Marca como usado, no filesystem, o espaço do arquivo recém-criado.
   */
  fs.bitmap[head_block] = 0;
  fs.fat[head_block] = -1;

  return dir;
}


void update_dir_block(int position, vdir_t dir) {
  string new_block;
  char size[9];
  char head[6];
  char name[NAME_SIZE];

  fs.file.seekp(position, ios::beg);

  debug("block", position / BLOCK_SIZE);
  debug("will write children", dir.children.size());

  new_block = "";
  for (auto child : dir.children) {
    sprintf(size, "%08zu", child.size);
    sprintf(head, "%05d", child.head);
    strcpy(name, child.name.c_str());

    debug("escrevendo", child.name);
    new_block.append(to_string(child.created));
    new_block.append(to_string(child.last_access));
    new_block.append(to_string(child.last_modified));
    new_block.append(child.is_dir ? "1" : "0");
    new_block.append(head);
    new_block.append(size);
    new_block.append(name);
  }

  /**
   * Um metadado de controle, com created nulo, é adicionado após o último filho
   * escrito no bloco, de modo a registrar o "fim" da lista de filhos. Isso não é
   * necessário quando um diretório possui o número máximo de filhos possível.
   */
  if (dir.children.size() != MAX_CHILDREN_PER_DIR) {
    new_block.append("000000000");
  }

  fs.bitmap[position] = 0;
  fs.fat[position] = -1;
  fs.blocks[position] = new_block;
}


void init_root_dir() {
  vdir_t root_dir, root_dir_parent;
  vattr_t root_attrs;
  time_t now;
  int root_pos;

  now = time(nullptr);
  root_pos = INITIAL_BLOCK;

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

  update_dir_block(root_pos, root_dir_parent);
  update_dir_block(root_pos+1, root_dir);
}


/**
 * Inicializa um filesystem vazio no arquivo apontado em fs. Isso inclui escrever lá
 * um bitmap vazio, uma quase–vazia tabela fat e os metadados do diretório /
 */
void init_empty_fs() {
  for (int i = 0; i < MAX_BLOCKS; i++) {
    fs.bitmap[i] = 1;
    fs.fat[i] = i;
    fs.blocks[i] = "";
  }

  write_bitmap_to_blocks();
  write_fat_to_blocks();
  init_root_dir();
}


/**
 * Interpreta um filesystem já existente, colocando em memória o seu conteúdo.
 */
void parse_fs() {
  char bit;
  char serialized[6];
  char block[BLOCK_SIZE + 1];

  fs.file.seekg(ios::beg);
  for (int i = 0; i < MAX_BLOCKS; i++) {
    bit = fs.file.get();
    fs.bitmap[i] = (int) bit-'0';
  }

  fs.file.seekg(BITMAP_SIZE * BLOCK_SIZE, ios::beg);
  for (int i = 0; i < MAX_BLOCKS; i++) {
    fs.file.get(serialized, 6);
    fs.fat[i] = stoi(serialized);
  }

  /**
   * Volta ao início do arquivo para ler todos os blocos, inclusive, aqueles
   * pertencentes ao bitmap e à tabela FAT.
   */
  fs.file.seekg(ios::beg);
  for (int i = 0; i < MAX_BLOCKS; i++) {
    fs.file.get(block, BLOCK_SIZE+1);
    fs.blocks[i] = string(block);
  }
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
    init_empty_fs();
  } else {
    parse_fs();
  }
}


/**
 * Salva o estado atual do sistema de arquivos, fechando-o.
 */
void unmount(cmd_t command) {
  write_blocks_to_fs();
  fs.file.close();
}


// void mkdir (cmd_t command) {
//   vdir_t* diretorio;

//   diretorio=malloc(sizeof(vdir_t*));

//   diretorio->name =command->argv[0];
//   for(i=1;command->args[i]!=null;i++){
//     diretorio->name+= " ";
//     diretorio->name+= command->args[i];
//   }
//   /*colocar no ARQUIVO */

// }


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


// void touch_file (cmd_t command){
//   vfile_t* arquivo;

//   /*procurar arquivo*/

//   if(/*achou arquivo na procura*/) time(&arquivo->last_access);
//   else{
//     arquivo=malloc(sizeof(vfile_t));
//     arquivo->name = command->args[0];
//     for(i=1;arquivo->args[i]!=null;i++){
//       arquivo->name+= " ";
//       arquivo->name+= command->args[i];
//     }

//     arquivo->size=?
//     time(&arquivo->created)
//   }

// }


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

void hook(cmd_t command) {
  int pos = stoi(command.args[0]);

  printf("--------- #%d\n", pos);
  printf("bitmap: %d\n", fs.bitmap[pos]);
  printf("fat: %d\n", fs.fat[pos]);
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
  try {
    if (command.cmd == "mount") {
      mount(command);
    } else if (command.cmd == "unmount") {
      unmount(command);
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
  debug("size", to_string(start).length());

  while (1) {
    command = prompt_command();

    if (command.cmd == "sai") break;
    execute(command);
  }
  // salva_arquivos();
  return 1;
}
