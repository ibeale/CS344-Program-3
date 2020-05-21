/*
Written By Isaac Beale
For CS 344 SP19
Due 5/21/2020
*/
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#define STR_BUF 2048


// SIGINT Handler. When SIGINT is caught and this handler is active, exit.
void catchSIGINT(int signo){
    char* message = "\nCaught SIGINT\n";
    write(STDOUT_FILENO, message, 15);
    exit(0);
}


//Data structure for background processes. Works as a statically allocated dynamic array
struct backgroundProcessPIDs{
    int num_processes;
    int PIDs[100];
};


int main(){
    // Counting variables for various uses
    int i;
    int j;
    // Flag for background mode. Default is ON.
    int BGMode = 1;
    // Signal mask for a process, so that we can hold signals until we're ready to accept them
    // Used for catching SIGTSTP
    sigset_t proc_mask_set, oset;
    sigemptyset(&proc_mask_set);
    sigaddset(&proc_mask_set, SIGTSTP);

    // SIGTSTP handler. Declared inside of main because it needs access to BGMode.
    // Toggles BGMode when SIGTSTP is caught.
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

    // Sigaction structs for various uses in the program
    // Action to ignore incoming signals
    struct sigaction ignore_action = {0};
    ignore_action.sa_handler = SIG_IGN;
    sigfillset(&ignore_action.sa_mask);
    
    // Sets the default action for SIGINT to be ignored
    struct sigaction SIGINT_action = {0};
    SIGINT_action.sa_handler = catchSIGINT;
    sigfillset(&SIGINT_action.sa_mask);
    sigaction(SIGINT, &ignore_action, NULL);

    // Sets the defaul action for SIGTSTP to be the catchSIGTSTP function.
    struct sigaction SIGTSTP_action = {0};
    SIGTSTP_action.sa_handler = catchSIGTSTP;
    sigfillset(&SIGTSTP_action.sa_mask);
    sigaction(SIGTSTP, &SIGTSTP_action, NULL);

    int backgroundFlag = 0;
    struct backgroundProcessPIDs BGPids;
    BGPids.num_processes = 0;

    char* args[513]; //513 to include the function call + 512 arguments. Will store pointers to argument strings.

    char* buffer;
    buffer = (char*) malloc(STR_BUF); // Buffer length set to 2048. Used as input buffer
    int num_args;
    int outputfileloc;
    int inputfileloc;
    
    // Flags to determine if we're redirecting input
    int outputflag = 0;
    int inputflag = 0;
    ssize_t buffer_size = STR_BUF;
    ssize_t inputsize = 0;

    // Arbitrary exit status so we know that things are being changed.
    int FGExitStatus = -5;
    
    // I didnt end up using this, but it could be potentially used to exit the shell otherwise.
    int exitflag = 0;
    do{
        // Set flags to their defaults
        backgroundFlag = 0;
        outputflag = 0;
        inputflag = 0;
        int childExitStatus = -5;
        

        // Function to get user input.
        while(1){

            // When the command line is refreshed, iterate through all of the running background processes
            // and check if they're done.
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

            // Reset the previous arguments array.
            for(i = 0; i < 513; i++){
                args[i] = NULL;
            }
            memset(buffer, '\0', STR_BUF);
            printf(": ");
            fflush(stdout);

            // Using fgets() here instead of getline() because getline() was causing problems with special characters
            // like ctrl+C ^C
            if(fgets(buffer,STR_BUF,stdin)!= NULL){
                break;
                fflush(stdout);
            };
            

        }

        // Create the delimiter for strtok
        char* delim = " \n";

        // Get the first argument from the buffer. They should be separated by a space character.
        char* arg = strtok(buffer, delim);

        // Counter for number of args.
        int num_args = 0;
        char* newarg;

        // If we get through all the arguments, strtok will return NULL and we'll break from the loop
        // I parse the arguments as strtok finds them in some cases.
        while(arg != NULL){

            // If we find the "$$" string somewhere in the argument,
            // create a new string that contains whatever preceded $$ in the argument
            // append the current process ID to the new string
            // and append the rest of the argument
            if(strstr(arg, "$$") != NULL){
                char pid[20];
                // New string to hold new argument
                newarg = malloc(STR_BUF);
                memset(pid,'\0', 20);
                memset(newarg,'\0', STR_BUF);
                // pid containts the process ID as a string
                sprintf(pid, "%d", getpid());
                int newargpos = 0;
                // Iterate through both old argument and newargument strings
                for(j = 0; j < strlen(arg);j++){
                    // If we find the $ symbol, append PID to the string instead of $
                    if(arg[j] == '$'){
                        strcat(newarg, pid);
                        // Set the iterating variable for the newargument to the new length of the string with the PID appended.
                        newargpos = strlen(newarg);
                        // Skip the next dollar symbol in the original string
                        j+=2;
                    }
                    newarg[newargpos] = arg[j];
                    newargpos++;
                }
                // strcpy doesnt work because arg is not the same size as the newarg, so I opted for strdup.
                arg = strdup(newarg);
                free(newarg);
            }

            // If theres a comment, break as everything else after this doesn't matter
            else if(arg[0] == '#'){
                break;
            }

            // If we find this character, the next argument is the output location so store that value for later.
            else if(arg[0] == '>'){
                outputfileloc = num_args + 1;
                outputflag = 1;
            }

            // Similar as above
            else if(arg[0] == '<'){
                inputfileloc = num_args + 1;
                inputflag = 1;
            }
            args[num_args] = arg;
            arg = strtok(NULL, delim);
            num_args++;
        }
         //If the user just enters a blank space then strtok returns null and num_args gets messed up.
        if(num_args == 0){
            args[0] = " ";
            num_args++;
        }

        // check if last argument is &, and set background flag if bgmode is on.
        if(!strcmp(args[num_args-1], "&")){
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
        
        // The following are the built in commands
        // CD changes the directory.
        if(strcmp("cd", args[0])==0){
            if(args[1])
                chdir(args[1]);
            else
                chdir(getenv("HOME"));
            fflush(stdout);
        }

        // Status reads FGExitStatus. Will return -5 if status is the first thing run.
        else if(strcmp("status", args[0])==0){
            if (WIFEXITED(FGExitStatus)){
                printf("exit value %d\n",WEXITSTATUS(FGExitStatus));
            }
            else{
                printf("Process was terminated by signal %d.\n", WTERMSIG(FGExitStatus));
            }
        }

        // Free the buffer we allocated and exit the program
        else if(strcmp("exit", args[0]) == 0){
            free(buffer);
            exit(0);
        }
        // Run all other commands with fork() and execvp()
        else{
            pid_t spawnpid = -5;
            spawnpid = fork();
            switch(spawnpid){
                // If spawnpid is -1 there was some error forking.
                case -1: 
                    printf("Hull Breach! \n");
                    fflush(stdout);
                    exit(1);break;
                // If its a child, then we turn it into an execvp() process and execute whatever is in the arguments
                case 0:
                    // Set the signal handlers appropriately. In this case, SIGINT should kill a child process, and SIGTSTP shouldn't do anything.
                    sigaction(SIGINT, &SIGINT_action, NULL);
                    sigaction(SIGTSTP, &ignore_action, NULL);

                    // Check input and output flags and redirect accordingly
                    if(inputflag){
                        int inputTarget = open(args[inputfileloc], O_RDONLY);
                        if(inputTarget == -1){
                            printf("cannot open %s for input\n", args[inputfileloc]);
                            exit(1);
                        }
                        dup2(inputTarget, 0);

                        // Turn the redirection arrow to NULL and the argument to NULL
                        // For example if the command was ls > junk, what would be sent to exec is ls NULL NULL since exec doesnt take those type of arguments
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

                    // If input and output flags were not set and the background flag is raised redirect input and output to dev/null.
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

                    // If during parsing something wierd happened, I handled this by changing the first argument to a space
                    // The child will exit with status 0 which is okay in this case.
                    if(!strcmp(args[0]," ")){
                        exit(0);
                    }

                    // If we get here, then lets run the command.
                    execvp(args[0], args);
                    fflush(stdout);
                    exit(1);
                    break;
                default: 
                    // The parent will wait for the child process to exit.
                    // Set the parent to ignore SIGINT.
                    sigaction(SIGINT, &ignore_action, NULL);

                    if(backgroundFlag == 0){
                        // Queue up any SIGTSTP commands that are incoming using a process mask
                        sigprocmask(SIG_BLOCK, &proc_mask_set, &oset);
                        // Wait for the process to finish
                        waitpid(spawnpid, &FGExitStatus, 0);
                        // Remove the mask and allow SIGTSTP commands to flow.
                        sigprocmask(SIG_UNBLOCK, &proc_mask_set, &oset);

                        // Handling the return values.
                        if (WIFSIGNALED(FGExitStatus)){
                            printf("Process was terminated by signal %d.\n", WTERMSIG(FGExitStatus));
                        }
                        else if(WEXITSTATUS(FGExitStatus) == 1){
                            printf("Error executing command.\n", args[0]);
                        }
                    }

                    // If it's a background command, append it to the running background processes and increment the counter.
                    // Tell the user that the process has started
                    else{
                        printf("Background process %d has started.\n", spawnpid);
                        BGPids.PIDs[BGPids.num_processes] = spawnpid;
                        BGPids.num_processes++;
                    }
            }

            // Restore original signal functions just in case.
            sigaction(SIGINT, &ignore_action, NULL);
            sigaction(SIGTSTP, &SIGTSTP_action, NULL);
            
        }
    }while(exitflag == 0);
    return 0;
}