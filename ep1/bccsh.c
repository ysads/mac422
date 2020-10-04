#include<stdio.h>
#include<stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include<unistd.h>
#include<string.h>
#include<errno.h>

extern int errno;

/*type_prompt escreve o prompt do shell.*/
void type_prompt (){
  char username[30], cwd[100];
  getcwd(cwd, 100);
  getlogin_r(username,30);
  printf("{%s@%s } ", username, cwd);
}

int main (){

  while (1) {
    type_prompt();
    /*command le a entrrada do shell e vê se é um dos comandos possíveis*/
    char *command;
    int a;
    size_t command_size = 100;
    command = (char *)malloc(command_size * sizeof(char));
    getline(&command, &command_size, stdin);
    /*cria uma pasta cujo nome está guardado em "copy"*/
    if(!strncmp(command, "mkdir", 5)){
      char copy[30];
      int size=strlen(command);
      strncpy(copy, command+6, size-7);
      if(mkdir(copy, 0700)){
        /*se criar a pasta não funcionou, diz qual foi o erro*/
        perror("Não foi possível criar a pasta. Erro");
      }
    }
    /*manda o sinal kill para um pid com o valor guardado em copy*/
    else if(!strncmp(command, "kill -9", 7)){
      char copy[30];
      int pid;
      strcpy(copy, command+8);
      pid=atoi(copy);
      if(kill(pid,SIGKILL)==-1){
        /*se houve um erro com o kill, diz qual*/
        perror("Não foi possível executar o kill. Erro");
      }
    }
    /*cria um link simbólico com o nome guardado em linkpath,
      que leva para o lugar guardado em target*/
    else if(!strncmp(command, "ln -s", 4)){
      char copy[60], target[30], linkpath[30];
      int i;
      strcpy(copy, command+6);
      for(i=0; i<60 && copy[i]!=' '; i++);
      strcpy(linkpath, copy+i+1);
      strncpy(target, copy, i);
      if(symlink(target, linkpath)==-1){
        /*Se não foi possivel fazer o link simbólico, explicita o erro*/
        perror("Não foi possível criar um link simbólico. \nErro");
      }
    }
    /*executa os comandos em paralelo*/
    else{
      if (fork() != 0) {
        /* Codigo do pai */
        waitpid(-1,NULL,0);
      }
      else {
        /* Codigo do filho */
        char* ep[]={"","","","",""};
        int i, size, begin, end, arg;
        char aux[5][100]={"","","","",""};
        size=strlen(command);
        end=-1;
        arg=0;
        for(i=0; i<size;i++){
          if(command[i]==' '|| i==size-1){
            begin=end+1;
            end=i;
            i=i;
            strncpy(aux[arg], command+begin, end-begin);
            ep[arg]=aux[arg];
            arg++;
          }
        }
        ep[arg]=NULL;
        execve(ep[0], ep, 0);
        perror("Erro executando ep1");
      }
    }
    /*como command foi mallocado, libera a memoria*/
    free(command);
  }
}
