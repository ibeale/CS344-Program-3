#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#define STR_BUF 2048

void catchSIGINT(int signo){
    char* message = "Caught SIGINT\n";
    write(STDOUT_FILENO, message, 14);
    exit(0);
}

struct backgroundProcessPIDs{
    int num_processes;
    int PIDs[100];
};


int main(){
    int i;
    int j;
    int backgroundFlag = 0;
    struct backgroundProcessPIDs BGPids;
    BGPids.num_processes = 0;
    struct sigaction SIGINT_action = {0};
    SIGINT_action.sa_handler = catchSIGINT;
    sigfillset(&SIGINT_action.sa_mask);
    sigaction(SIGINT, &SIGINT_action, NULL);
    char* args[513]; //513 to include the function call + 512 arguments.
    char* buffer;
    buffer = (char*) malloc(STR_BUF);
    int num_args;
    int outputfileloc;
    int inputfileloc;
    int outputflag = 0;
    int inputflag = 0;
    ssize_t buffer_size = STR_BUF;
    ssize_t inputsize = 0;
    
    int exitflag = 0;
    do{
        backgroundFlag = 0;
        int childExitStatus = -5;
        for(i=0; i < BGPids.num_processes; i++){
            int finished = waitpid(BGPids.PIDs[i], &childExitStatus, WNOHANG);
            if(finished){
                BGPids.num_processes--;
            }
        }
        while(1){
            outputflag = 0;
            inputflag = 0;
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
        char* newarg;
        while(arg != NULL){
            if(strstr(arg, "$$") != NULL){
                char pid[20];
                newarg = malloc(STR_BUF);
                memset(pid,'\0', 20);
                memset(newarg,'\0', STR_BUF);
                sprintf(pid, "%d", getpid());
                int newargpos = 0;
                for(j = 0; j < strlen(arg);j++){
                    if(arg[j] == '$'){
                        strcat(newarg, pid);
                        newargpos = strlen(newarg);
                        j+=2;
                    }
                    newarg[newargpos] = arg[j];
                    newargpos++;
                }
                arg = strdup(newarg);
                free(newarg);
            }
            else if(arg[0] == '#'){
                break;
            }
            else if(arg[0] == '>'){
                outputfileloc = num_args + 1;
                outputflag = 1;
            }
            else if(arg[0] == '<'){
                inputfileloc = num_args + 1;
                inputflag = 1;
            }
            args[num_args] = arg;
            arg = strtok(NULL, delim);
            num_args++;
        }

        if(!strcmp(args[num_args-1], "&")){ // check if last argument is &, and set background flag.
            backgroundFlag = 1;
            args[num_args-1] = NULL;
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
            spawnpid = fork();
            switch(spawnpid){
                case -1: // If spawnpid is -1 there was some error forking.
                    printf("Hull Breach! \n");
                    fflush(stdin);
                    exit(1);break;
                case 0: //If its a child, then we turn it into an execvp() process and execute whatever is in the arguments
                    if(inputflag){
                        int inputTarget = open(args[inputfileloc], O_RDONLY);
                        dup2(inputTarget, 0);
                        args[inputfileloc - 1] = NULL;
                        args[inputfileloc] = NULL;
                        close(inputTarget);
                    }
                    if(outputflag){
                        int outputTarget = open(args[outputfileloc], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                        dup2(outputTarget, 1);
                        args[outputfileloc - 1] = NULL;
                        args[outputfileloc] = NULL;
                        close(outputTarget);
                    }
                    if(outputflag == 0 && backgroundFlag == 1){
                        int devnull = open("/dev/null", O_WRONLY | O_CREAT | O_TRUNC, 0644);
                        dup2(devnull, 1);
                        close(devnull);
                    }
                    if(inputflag == 0 && backgroundFlag == 1){
                        int devnull = open("/dev/null", O_RDONLY);
                        dup2(devnull, 0);
                        close(devnull);
                    }
                    execvp(args[0], args);
                    
                    fflush(stdin);
                    exit(0);
                    break;
                default: // The parent will wait for the child process to exit.
                    if(backgroundFlag == 0)
                        waitpid(spawnpid, &childExitStatus, 0);
                    else{
                        BGPids.PIDs[BGPids.num_processes] = spawnpid;
                        BGPids.num_processes++;
                    }
            }
            
        }
    }while(exitflag == 0);
    return 0;
}