#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#define STR_BUF 2048

void catchSIGINT(int signo){
    char* message = "\nCaught SIGINT\n";
    write(STDOUT_FILENO, message, 15);
    exit(0);
}




struct backgroundProcessPIDs{
    int num_processes;
    int PIDs[100];
};


int main(){
    int i;
    int j;
    int BGMode = 1;
    sigset_t proc_mask_set, oset;
    sigemptyset(&proc_mask_set);
    sigaddset(&proc_mask_set, SIGTSTP);
    void catchSIGTSTP(int signo){
        if(BGMode){
            char* message = "\nBackground Processes disabled.\n";
            write(STDOUT_FILENO, message, 32);
            BGMode = 0;

        }
        else{
            char* message = "\nBackground Processes enabled.\n";
            write(STDOUT_FILENO, message, 31);
            BGMode = 1;

        }
        fflush(stdout);
    }
    struct sigaction ignore_action = {0};
    ignore_action.sa_handler = SIG_IGN;
    sigfillset(&ignore_action.sa_mask);
    

    int backgroundFlag = 0;
    struct backgroundProcessPIDs BGPids;
    BGPids.num_processes = 0;

    struct sigaction SIGINT_action = {0};
    SIGINT_action.sa_handler = catchSIGINT;
    sigfillset(&SIGINT_action.sa_mask);
    sigaction(SIGINT, &ignore_action, NULL);

    struct sigaction SIGTSTP_action = {0};
    SIGTSTP_action.sa_handler = catchSIGTSTP;
    sigfillset(&SIGTSTP_action.sa_mask);
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);



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
    int FGExitStatus = -5;
    
    int exitflag = 0;
    do{
        backgroundFlag = 0;
        int childExitStatus = -5;
        while(1){
            for(i=0; i < BGPids.num_processes; i++){
                int finished = waitpid(BGPids.PIDs[i], &childExitStatus, WNOHANG);
                if(finished){
                    printf("Process %d has completed\n", BGPids.PIDs[i]);
                    if (WIFEXITED(childExitStatus)){
                        printf("exit value %d\n",WEXITSTATUS(childExitStatus));
                    }
                    else{
                        printf("Process was terminated by signal %d.\n", WTERMSIG(childExitStatus));
                    }
                    BGPids.num_processes--;
                }
            }
            outputflag = 0;
            inputflag = 0;
            for(i = 0; i < 513; i++){
                args[i] = NULL;
            }
            memset(buffer, '\0', STR_BUF);
            printf(": ");
            fflush(stdout);
            if(fgets(buffer,STR_BUF,stdin)!= NULL){
                break;
                fflush(stdout);
            };
            

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
        if(num_args == 0){ //If the user just enters a blank space then strtok returns null and num_args gets messed up.
            args[0] = " ";
            num_args++;
        }

        if(!strcmp(args[num_args-1], "&")){ // check if last argument is &, and set background flag.
            if((num_args - 1) > 0){
                args[num_args-1] = NULL;
                if(BGMode){
                    backgroundFlag = 1;
                }
                else{
                    backgroundFlag = 0;
                }
            }
        }
        
        if(strcmp("cd", args[0])==0){
            if(args[1])
                chdir(args[1]);
            else
                chdir(getenv("HOME"));
            fflush(stdout);
        }
        else if(strcmp("status", args[0])==0){
            if (WIFEXITED(FGExitStatus)){
                printf("exit value %d\n",WEXITSTATUS(FGExitStatus));
            }
            else{
                printf("Process was terminated by signal %d.\n", WTERMSIG(FGExitStatus));
            }
        }
        else if(strcmp("exit", args[0]) == 0){
            exit(0);
        }
        // Run all other commands with fork() and execvp()
        else{
            pid_t spawnpid = -5;
            spawnpid = fork();
            switch(spawnpid){
                case -1: // If spawnpid is -1 there was some error forking.
                    printf("Hull Breach! \n");
                    fflush(stdout);
                    exit(1);break;
                case 0: //If its a child, then we turn it into an execvp() process and execute whatever is in the arguments
                    sigaction(SIGINT, &SIGINT_action, NULL);
                    sigaction(SIGTSTP, &ignore_action, NULL);
                    if(inputflag){
                        int inputTarget = open(args[inputfileloc], O_RDONLY);
                        if(inputTarget == -1){
                            printf("cannot open %s for input\n", args[inputfileloc]);
                            exit(1);
                        }
                        dup2(inputTarget, 0);
                        args[inputfileloc - 1] = NULL;
                        args[inputfileloc] = NULL;
                        close(inputTarget);
                    }
                    if(outputflag){
                        int outputTarget = open(args[outputfileloc], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                        if(outputTarget == -1){
                            exit(1);
                        }
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
                    if(!strcmp(args[0]," ")){
                        exit(0);
                    }
                    execvp(args[0], args);
                    fflush(stdout);
                    exit(1);
                    break;
                default: // The parent will wait for the child process to exit.
                    sigaction(SIGINT, &ignore_action, NULL);
                    if(backgroundFlag == 0){
                        sigprocmask(SIG_BLOCK, &proc_mask_set, &oset);
                        waitpid(spawnpid, &FGExitStatus, 0);
                        sigprocmask(SIG_UNBLOCK, &proc_mask_set, &oset);
                        if (WIFSIGNALED(FGExitStatus)){
                            printf("Process was terminated by signal %d.\n", WTERMSIG(FGExitStatus));
                        }
                        else if(WEXITSTATUS(FGExitStatus) == 1){
                            printf("%s: Command not found.\n", args[0]);
                        }
                    }
                    else{
                        printf("Background process %d has started.\n", spawnpid);
                        BGPids.PIDs[BGPids.num_processes] = spawnpid;
                        BGPids.num_processes++;
                    }
            }
            sigaction(SIGINT, &ignore_action, NULL);
            sigaction(SIGTSTP, &SIGTSTP_action, NULL);
            
        }
    }while(exitflag == 0);
    return 0;
}