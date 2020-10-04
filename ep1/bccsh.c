#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "bccsh.h"

extern int errno;

/**
 * type_prompt escreve o prompt do shell.
 */
void type_prompt() {
    char username[ARG_LEN], cwd[CMD_LEN];

    getcwd(cwd, CMD_LEN);
    getlogin_r(username, ARG_LEN);
    printf("{%s@%s} ", username, cwd);
}

/*cria uma pasta cujo nome está guardado em "copy"*/
void create_dir(char* command) {
    char copy[ARG_LEN];
    int size = strlen(command);
    
    strncpy(copy, command+6, size-7);

    if (mkdir(copy, 0700)) {
        /*se criar a pasta não funcionou, diz qual foi o erro*/
        perror("Não foi possível criar a pasta. Erro");
    }
}


/*
 * manda o sinal kill para um pid com o valor guardado em copy
 */
void kill_ps(char* command) {
    char copy[ARG_LEN];
    int pid;
    
    strcpy(copy, command+8);
    pid = atoi(copy);
    
    if (kill(pid, SIGKILL) == -1) {
        /*se houve um erro com o kill, diz qual*/
        perror("Não foi possível executar o kill. Erro");
    }
}


/**
 * cria um link simbólico com o nome guardado em linkpath, que leva
 * para o lugar guardado em target
 */
void create_symlink(char* command) {
    char copy[60], target[ARG_LEN], linkpath[ARG_LEN];
    int i;

    strcpy(copy, command+6);

    for (i = 0; i < 60 && copy[i] != ' '; i++);
    
    strcpy(linkpath, copy+i+1);
    strncpy(target, copy, i);
    
    if(symlink(target, linkpath)==-1){
        /*Se não foi possivel fazer o link simbólico, explicita o erro*/
        perror("Não foi possível criar um link simbólico. \nErro");
    }
}


/* executa os comandos em paralelo */
void execute(char* command) {
    if (fork() != 0) {
        /* Codigo do pai */
        waitpid(-1,NULL,0);
    }
    else {
        /* Codigo do filho */
        char* ep[] = {"","","","",""};
        int i, size, begin, end, arg;
        char aux[5][100] = {"","","","",""};
        
        size = strlen(command);
        end = -1;
        arg = 0;
        
        for (i = 0; i < size; i++) {
            if (command[i] == ' ' || i == size-1){
                begin = end+1;
                end = i;
                strncpy(aux[arg], command+begin, end-begin);
                ep[arg] = aux[arg];
                arg++;
            }
        }
        ep[arg] = NULL;
        execve(ep[0], ep, 0);
        perror("Erro executando o comando");
    }
}


int main() {
    while (1) {
        char *command;
        size_t command_size = CMD_LEN;

        type_prompt();

        /*
         * command le a entrrada do shell e vê se é um dos comandos possíveis
         */
        command = (char *) malloc(command_size * sizeof(char));
        getline(&command, &command_size, stdin);

        if(!strncmp(command, "mkdir", 5)) {
            create_dir(command);
        }
        else if(!strncmp(command, "kill -9", 7)) {
            kill_ps(command);
        }
        else if(!strncmp(command, "ln -s", 4)) {
            create_symlink(command);
        }
        else {
            execute(command);
        }
        
        /* como command foi alocado com malloc, libera a memoria */
        free(command);
    }
}
