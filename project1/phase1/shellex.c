/* $begin shellmain */
#include "csapp.h"
#include<errno.h>
#define MAXARGS   128

/* history constant */
int historyCount;
#define HISTFILE "/.history"
char histfilePath[1024];

/* Function prototypes */
void eval(char *cmdline);
int parseline(char *buf, char **argv);
int builtin_command(char **argv);


/* history function */
void historyInit(void); /* history update from file */
void history_command(int type); /* history command (history, !!, !#) */
void history_add(char command[]);/* add command history */

/* signal handler */
void unblock_signals(void);
void sigint_handler(int signum);
void sigquit_handler(int signum);
void sigchld_handler(int signum);

int main() {
    char cmdline[MAXLINE]; /* Command line */
    
    /* save history file path */
    getcwd(histfilePath, 1024);
    strcat(histfilePath, HISTFILE);

    
    sigset_t mask;// blocksigset
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);// sigint add
    sigaddset(&mask, SIGQUIT);// sigquit add
    sigprocmask(SIG_BLOCK, &mask, NULL);
    signal(SIGCHLD, sigchld_handler);
    /* history */
    historyInit();


    while (1) {
    /* Read */
    printf("CSE4100-MP-P1> ");
    fgets(cmdline, MAXLINE, stdin);
    if (feof(stdin))
        exit(0);
        
    
    /* Evaluate and Save history */
    if (cmdline[0] != '!') {
        history_add(cmdline);
        eval(cmdline);
    }
    else {
        eval(cmdline);
    }
    }
}
/* $end shellmain */
  
/* $begin eval */
/* eval - Evaluate a command line */
void eval(char *cmdline)
{
    char *argv[MAXARGS]; /* Argument list execve() */
    char buf[MAXLINE];   /* Holds modified command line */
    int bg;              /* Should the job run in bg or fg? */
    pid_t pid;           /* Process id */
    int status;
    strcpy(buf, cmdline);
    bg = parseline(buf, argv);
    if (argv[0] == NULL)
        return;   /* Ignore empty lines */
    
    if (!builtin_command(argv)) { //quit -> exit(0), & -> ignore, other -> run
        if ((pid=Fork()) == 0) {
            unblock_signals(); // receive signals in child process
            if (execvp(argv[0], argv) < 0) {    //ex) /bin/ls ls -al &
                printf("%s: Command not found.\n", argv[0]);
                exit(0);
            }
        }
        waitpid(pid, &status, 0);

    /* Parent waits for foreground job to terminate */
    if (!bg){
        
    }
    else//when there is backgrount process!
        printf("%d %s", pid, cmdline);
    }
    return;
}

/* If first arg is a builtin command, run it and return true */
int builtin_command(char **argv)
{
    if (!strcmp(argv[0], "quit")) /* quit command */
        exit(0);
    if (!strcmp(argv[0], "exit")) /* exit */
        exit(0);
    if (!strcmp(argv[0], "&"))    /* Ignore singleton & */
        return 1;

    if (!strcmp(argv[0], "history")){
        history_command(0);
        return 1;
    }
    if (argv[0][0] == '!') {
        if(argv[0][1] == '!') {
            history_command(-1);
            return 1;
        }
        else {
            char str[MAXLINE];
            strcpy(str, argv[0]+1);
            history_command(atoi(str));
            return 1;
        }
    }
    if (!strcmp(argv[0], "cd")){
        if (argv[1]) {
            if(chdir(argv[1])) {
                printf("cd: %s: No such file or directory^^\n", argv[1]);
            }
        }
        else {
            chdir(getenv("HOME"));
        }
        return 1;
    }
    return 0;                     /* Not a builtin command */
}
/* $end eval */

/* $begin parseline */
/* parseline - Parse the command line and build the argv array */
int parseline(char *buf, char **argv)
{
    char *delim;         /* Points to first space delimiter */
    int argc;            /* Number of args */
    int bg;              /* Background job? */

    buf[strlen(buf)-1] = ' ';  /* Replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* Ignore leading spaces */
    buf++;

    /* Build the argv list */
    argc = 0;
    while ((delim = strchr(buf, ' '))) {
        argv[argc++] = buf;
        *delim = '\0';
        buf = delim + 1;
        while (*buf && (*buf == ' ')) /* Ignore spaces */
            buf++;
    }
    argv[argc] = NULL;
    
    if (argc == 0)  /* Ignore blank line */
    return 1;

    /* Should the job run in the background? */
    if ((bg = (*argv[argc-1] == '&')) != 0)
    argv[--argc] = NULL;

    return bg;
}
/* $end parseline */


void historyInit(void) {
    FILE *fp = fopen(histfilePath, "r");
    if (fp) {
        historyCount = 0;
        char line[MAXLINE];
        while (fgets(line, sizeof(line), fp)) {
            historyCount ++;
            }
    }
    else {
        fp = fopen(histfilePath, "w");
    }
    fclose(fp);
    
}

void history_command(int type) {
    if (type==0){
        FILE *fp = fopen(histfilePath, "r");
        char line[MAXLINE];
        while (fgets(line, sizeof(line), fp)) {
            printf("%s", line);
        }
        fclose(fp);
    }
    else if (type==-1){ // !! command
        FILE *fp = fopen(histfilePath, "r");
        char line[MAXLINE];
        char str[MAXLINE];
        int count = 0;
        while (fgets(line, sizeof(line), fp)) {
            count ++;
            if (count == historyCount) {
                strcpy(str, line+6);
                printf("%s",str);
                eval(str);
            }
        }
        history_add(str);
        fclose(fp);
    }
    else { // !# command
        FILE *fp = fopen(histfilePath, "r");
        char line[MAXLINE];
        char str[MAXLINE];
        int count = 0;
        while (fgets(line, sizeof(line), fp)) {
            count ++;
            if (count == type) {
                strcpy(str, line+6);
                printf("%s",str);
                eval(str);
            }
        }
        history_add(str);
        fclose(fp);
    }

}

void history_add(char command[]) {
    FILE *fp = fopen(histfilePath, "r");
    char line[MAXLINE];
    char str[MAXLINE];
    int count = 0;
    while (fgets(line, sizeof(line), fp)) {
        count ++;
        if (count == historyCount) {
            sprintf(str, "%-5d %s",historyCount, command);
            if(!strcmp(line, str)) {
                return;
            }
        }
    }
    fclose(fp);
    
    historyCount++;
    fp = fopen(histfilePath, "a+");
    fprintf(fp, "%-5d %s", historyCount, command);
    fclose(fp);
    return;
}



void sigint_handler(int signum){
    // not need in phase1
}
void sigquit_handler(int signum){
   // not need in phase1
}

void sigchld_handler(int signum){
    int status;
    pid_t pid;
    
    while((pid = waitpid(-1, &status, WNOHANG)) > 0){ // reaping child process
        if (WIFEXITED(status)) { // child terminated normally
//            printf("Child process %d terminated normally with exit status %d\n", pid, WEXITSTATUS(status));
            }
        else if (WIFSIGNALED(status)) { // child terminated abnormally
//            printf("Child process %d terminated abnormally\n", pid);
            }
        }
    
    if(pid < 0) { // waitpid fail
        if(errno != ECHILD){
            perror("waitpid failed");
            exit(1);
        }
    }
}

void unblock_signals(void) {
    sigset_t mask;
    sigemptyset(&mask);          // initialize mask
    sigaddset(&mask, SIGINT);    // remove SIGINT
    sigaddset(&mask, SIGQUIT);   // remove SIGQUIT
    sigprocmask(SIG_UNBLOCK, &mask, NULL);  // remove signals block
    return;
}
