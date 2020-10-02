#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/wait.h>
#include<sys/stat.h>
#include<signal.h>
#include<unistd.h>
#include<string.h>
#include<errno.h>

extern int errno;

/*type_prompt escreve o prompt do shell.*/
void type_prompt() {
  char username[30], cwd[100];
  getcwd(cwd, 100);
  getlogin_r(username, 30);
  printf("{%s@%s} ", username, cwd);
}

int main () {
  while (1) {
    type_prompt();
    /*command le a entrada do shell e vê se é um dos comandos possíveis*/
    char *command;
    size_t command_size = 100;
    command = (char *)malloc(command_size * sizeof(char));
    getline(&command, &command_size, stdin);

    /*cria uma pasta cujo nome está guardado em "copy"*/
    if(!strncmp(command, "mkdir", 5)) {
      char copy[30];
      strcpy(copy, command+6);
      printf("%s", copy);
      if(mkdir(copy, 0700)){
        /*se criar a pasta não funcionou, diz qual foi o erro*/
        perror("Não foi possível criar a pasta");
      }
    }
    /*manda o sinal kill para um pid com o valor guardado em copy*/
    else if(!strncmp(command, "kill -9", 7)) {
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
    else if(!strncmp(command, "ln -s", 4)) {
      char copy[60], target[30], linkpath[30];
      int i;
      strcpy(copy, command+6);
      for(i=0; i<60 && copy[i]!=' '; i++);
      strcpy(linkpath, copy+i+1);
      strncpy(target, copy, i);
      if(symlink(target, linkpath)==-1) {
        /*Se não foi possivel fazer o link simbólico, explicita o erro*/
        perror("Não foi possível criar um link simbólico. \nErro");
      }
    }
    /*mostra o uso de disco da pasta em que o programa está rodando*/
    else if(strcmp(command, "/usr/bin/du -hs")==10) {
      if (fork() != 0) {
        /* Codigo do pai */
        waitpid(-1,NULL,0);
      }
      else {
        /* Codigo do filho */;
        char* du[]={"","","",""};
        char cwd[100];
        getcwd(cwd,100);
        du[0]="/usr/bin/du";
        du[1]="-hs";
        du[2]=cwd;
        du[3]=NULL;
        execve(du[0],du,0);
        /*se houve um problema na execução, diz qual é*/
        perror("Erro executando du");
      }
    }
    /*mostra a rota do seu IP até o host(google)*/
    else if(strcmp(command, "/usr/sbin/traceroute www.google.com.br")==10) {
      if (fork() != 0) {
        /* Codigo do pai */
        waitpid(-1,NULL,0);
      }
      else {
        /* Codigo do filho */
        char* traceroute[]={"","",""};
        traceroute[0]="/usr/bin/traceroute";
        traceroute[1]="www.google.com.br";
        traceroute[2]=NULL;
        execve(traceroute[0],traceroute,0);
        /*se houver algum problema na execução, diz qual é*/
        perror("Erro executando traceroute");
      }
    }
    /*executa o ep1*/
    else if(!strncmp(command, "./ep1", 5)) {
      if(system(command)==-1)
        /*se houver algum problema na execução, diz qual é.*/
        perror("Erro executando ./ep1");
    }
    else printf("O shell não aceita este comando\n");
    /*como command foi mallocado, libera a memoria*/
    free(command);
  }
}
