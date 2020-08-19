#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <wait.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/stat.h>

#define BUFFER_SIZE 100

void sendToShell(char *com);
void handleCommand(char* com);
void handleJobs();
void handleHistory();
void saveJob(char *com, pid_t pid);
void handleCD(char* com);
void updateCD();

typedef struct {
    int pid;
    char name[100];
} Command;

Command jobs[BUFFER_SIZE];
Command history[BUFFER_SIZE];
int jobsCounter = 0;
int histCounter = 0;
char current[PATH_MAX] = "";
char prev[PATH_MAX] = "";

int main() {
    char inputBuf[BUFFER_SIZE];
    char dummy;
    printf("> ");
    scanf("%[^\n]s", inputBuf);
    scanf("%c", &dummy);
    while (strcmp(inputBuf, "exit") != 0) {
        handleCommand(inputBuf);
        // get next command:
        printf("> ");
        scanf("%[^\n]s", inputBuf);
        scanf("%c", &dummy);
    }
    printf("%d\n", getpid());
}

void handleCommand(char* com) {
    // check if command is cd:
    char temp[BUFFER_SIZE];
    strcpy(temp, com);
    char *str = strtok(temp, " ");
    if (!strcmp(str,"cd")) {
        handleCD(com);
        saveJob(com, getpid());
        return;
    }
    // other command:
    pid_t pid = fork();
    if (pid == -1) {
        fprintf(stderr, "Error in system call\n");
    } else if (pid == 0) { // child:
        if (!strcmp(com, "jobs")) {
            handleJobs();
        } else if (!strcmp(com, "history")) {
            handleHistory();
        } else {
            printf("%d\n", getpid());
            sendToShell(com);
        }
    } else { // father:
        // ignore signals from child to prevent zombies:
        signal(SIGCHLD,SIG_IGN);
        if (com[strlen(com) - 1] != '&') {
            // wait for child to finish (foreground):
            int returnStatus;
            waitpid(pid, &returnStatus, 0);
        } else {
            // delete "&" from the command (background):
            com[strlen(com) - 2] = 0;
        }
        // save child's job:
        saveJob(com, pid);
    }
}

void sendToShell(char *com) {
    // prepare command for sending:
    char temp[BUFFER_SIZE];
    strcpy(temp, com);
    char* argv[BUFFER_SIZE];
    long argc = 0;
    char *str = strtok(temp, " ");
    while (str != NULL) {
        argv[argc++] = str;
        str = strtok(NULL, " ");
    }
    argv[argc] = NULL;
    // delete '&':
    if (strcmp(argv[argc - 1], "&") == 0) {
        argv[argc - 1] = NULL;
        argc--;
    }
    // strip echo from "":
    if (strcmp(argv[0], "echo") == 0) {
        if (argv[1][0] == '\"') {
            argv[1]++;
            char* last = argv[argc - 1];
            last[strlen(last)-1] = 0;
        }
    }
    char cmd[1024] = {0};
    system(cmd);
    execvp(argv[0], argv);
    // failed executing:
    fprintf(stderr, "Error in system call\n");
    exit(0);
}

void saveJob(char *com, pid_t pid) {
    // add to history&jobs:
    histCounter++;
    history[histCounter - 1].pid = pid;
    strcpy(history[histCounter - 1].name, com);
    jobsCounter++;
    jobs[jobsCounter - 1].pid = pid;
    strcpy(jobs[jobsCounter - 1].name, com);
}

void handleJobs() {
    int i;
    for (i = 0; i < jobsCounter; i++) {
        if (jobs[i].pid == 0) {
            continue;
        }
        // check if job is running:
        if ((kill(jobs[i].pid, 0)) || (jobs[i].pid == getppid()))  {
            // job is done, 'delete' from jobs array:
            jobs[i].pid = 0;
        } else {
            // job is running:
            printf("%d %s\n", jobs[i].pid, jobs[i].name);
        }
    }
    exit(0);
}

void handleHistory() {
    int i;
    for (i = 0; i < histCounter; i++) {
        printf("%d ", history[i].pid);
        printf("%s ", history[i].name);
        if ((kill(history[i].pid, 0)) || (history[i].pid == getppid())) {
            printf("%s", "DONE\n");
        } else {
            printf("%s", "RUNNING\n");
        }
    }
    printf("%d history RUNNING\n", getpid());
    exit(0);
}

void updateCD() {
    // update previous directory:
    memset(prev, 0, sizeof(prev));
    strcpy(prev, current);
    // update current directory:
    memset(current, 0, sizeof(current));
        if (getcwd(current, sizeof(current)) == NULL) {
        fprintf(stderr, "Error in system call\n");
    }
}

void handleCD(char* com) {
    // prepare command for sending:
    char tmp[BUFFER_SIZE];
    strcpy(tmp, com);
    char* argv[BUFFER_SIZE];
    argv[1] = NULL;
    argv[2] = NULL;
    long argc = 0;
    char *str = strtok(tmp, " ");
    while (str != NULL) {
        argv[argc++] = str;
        str = strtok(NULL, " ");
    }
    // check number of arguments:
    if (argv[2] != NULL) {
        fprintf(stderr, "Error: Too many arguments\n");
        return;
    }
    // print pid of calling process:
    printf("%d\n", getpid());
    memset(current, 0, sizeof(current));
    if (getcwd(current, sizeof(current)) == NULL) {
        fprintf(stderr, "Error in system call\n");
    }
    //cd, cd ~ : go to home directory:
    if ((argv[1] == NULL) || (!strcmp(argv[1], "~"))) {
        if (chdir(getenv("HOME")) == -1) {
            // cd failed:
            fprintf(stderr, "Error in system call\n");
        } else {
            // cd succeeded:
            updateCD();
        }
    //cd - : go to previous directory:
    } else if (!strcmp(argv[1], "-")) {
        // check if there's a previous directory:
        if (!strcmp(prev, "")) {
            return;
        } else {
            // there is a previous directory:
            if (chdir(prev) == -1) {
                fprintf(stderr, "Error in system call\n");
            } else {
                // cd succeeded:
                updateCD();
            }
        }
    // cd .. : go to parent directory:
    } else if (!strcmp(argv[1], "..")){
        if (chdir("..") == -1){
            // cd failed:
            fprintf(stderr, "Error in system call\n");
        } else {
            // cd succeeded:
            updateCD();
        }
    // cd PATH : go to given path:
    } else {
        char newPath[PATH_MAX] = "";
        if (argv[1][0] == '~') {
            argv[1]++;
            strcpy(newPath, getenv("HOME"));
            strcat(newPath, argv[1]);
        } else if (argv[1][0] == '-') {
            argv[1]++;
            strcpy(newPath, prev);
            strcat(newPath, argv[1]);
        } else {
            strcpy(newPath, argv[1]);
        }
        struct stat stats1;
        struct stat stats2;
        stat(newPath, &stats1);
        stat(newPath, &stats2);
        if (!stats1.st_mode & F_OK || S_ISDIR(stats2.st_mode)) {
            if (chdir(newPath) == -1) {
                fprintf(stderr, "Error in system call\n");
            } else {
                // cd succeeded:
                updateCD();
            }
        } else {
            fprintf(stderr, "Error: no such file or directory\n");
        }
    }
}
