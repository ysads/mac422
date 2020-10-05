#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define CMD_LEN 100
#define ARG_LEN 30

extern int errno;

/**
 * Writes the prompt on screen.
 */
void type_prompt() {
    char username[ARG_LEN], cwd[CMD_LEN];

    getcwd(cwd, CMD_LEN);
    getlogin_r(username, ARG_LEN);
    printf("{%s@%s} ", username, cwd);
}

/**
 * Creates a folder with the named store at copy.
 */
void create_dir(char* command) {
    char copy[ARG_LEN];
    int size = strlen(command);

    strncpy(copy, command+6, size-7);

    if (mkdir(copy, 0700)) {
        /**
         * Shows an error in case it can't create the folder.
         */
        perror("Não foi possível criar a pasta. Erro");
    }
}


/**
 * Sends a SIGKILL to the PID store at copy var. 
 */
void kill_ps(char* command) {
    char copy[ARG_LEN];
    int pid;

    strcpy(copy, command+8);
    pid = atoi(copy);

    if (kill(pid, SIGKILL) == -1) {
        /**
         * Shows an error in case it can't send the SIGKILL.
         */
        perror("Não foi possível executar o kill. Erro");
    }
}


/**
 * Creates a symbolic link with the name stored on linkpath targeting
 * the value at target var.
 */
void create_symlink(char* command) {
    char copy[60], target[ARG_LEN], linkpath[ARG_LEN];
    int i;

    strcpy(copy, command+6);

    for (i = 0; i < 60 && copy[i] != ' '; i++);
    
    strcpy(linkpath, copy+i+1);
    strncpy(target, copy, i);
    
    if(symlink(target, linkpath)==-1){
        /**
         * Fails in case it can't create the symbolic link, showing the error.
         */
        perror("Não foi possível criar um link simbólico. \nErro");
    }
}


/* executa os comandos em paralelo */
void execute(char* command) {
    if (fork() != 0) {
        /**
         * Parent process code.
         */
        waitpid(-1, NULL, 0);
    }
    else {
        /**
         * Child-process code.
         */
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

        /**
         * Reads a command from shell input and decides whether it's a shell
         * built-in or an external command.
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
        
        /**
         * Must be manually freed since it has been manually allocated.
         */
        free(command);
    }
}
