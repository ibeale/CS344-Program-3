#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

#define STR_BUF 2048

void catchSIGINT(int signo){
    char* message = "Caught SIGINT\n";
    write(STDOUT_FILENO, message, 14);
    exit(0);
}


int main(){
    int i;
    struct sigaction SIGINT_action = {0};
    SIGINT_action.sa_handler = catchSIGINT;
    sigfillset(&SIGINT_action.sa_mask);
    sigaction(SIGINT, &SIGINT_action, NULL);
    char* args[513]; //513 to include the function call + 512 arguments.
    char* buffer;
    buffer = (char*) malloc(STR_BUF);
    int num_args;
    ssize_t buffer_size = STR_BUF;
    ssize_t inputsize = 0;
    int exitflag = 0;
    do{
        while(1){
            for(i = 0; i < 513; i++){
                args[i] = NULL;
            }
            memset(buffer, '\0', STR_BUF);
            printf(": ");
            fflush(stdin);
            inputsize = getline(&buffer, &buffer_size, stdin);
            if(inputsize > 1){
                break;
            }
        }
        char* delim = " \n";
        char* arg = strtok(buffer, delim);
        int num_args = 0;
        while(arg != NULL){
            args[num_args] = arg;
            arg = strtok(NULL, delim);
            num_args++;
        }
        
        for(i = 0;i<num_args;i++){
            printf("%s\n", args[i]);
            fflush(stdin);
        }
        if(strcmp("cd", args[0])==0){
            if(args[1])
                chdir(args[1]);
            else
                chdir(getenv("HOME"));
        }
        // Run all other commands with fork() and execvp()
        else{
            pid_t spawnpid = -5;
            int childExitStatus = -5;
            spawnpid = fork();
            switch(spawnpid){
                case -1: // If spawnpid is -1 there was some error forking.
                    printf("Hull Breach! \n");
                    fflush(stdin);
                    exit(1);break;
                case 0: //If its a child, then we turn it into an execvp() process and execute whatever is in the arguments
                    execvp(args[0], args);
                    fflush(stdin);
                    exit(0);
                    break;
                default: // The parent will wait for the child process to exit.
                    waitpid(spawnpid, &childExitStatus, 0);
            }
        }
    }while(exitflag == 0);
    return 0;
}