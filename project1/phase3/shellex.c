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
void sigtstp_handler(int signum);

/* pipe */
void mypipe(char** argv, int bg, char* command);
int checkPipeCommand(char** argv);

/* background and foreground */
int bg;
pid_t fg_pid;
char fg_cmdline[MAXLINE];
#define MAX_JOBS 100 // max jobs count


typedef struct job {
    pid_t pid;
    int status; // 1:ruuning 2:suspended
    char command[MAXLINE];
} job;

job jobs[MAX_JOBS]; // jobs array
int num_jobs = 0;   // current jobs num
void show_jobs(void);
void put_jobs(pid_t pid, char* command, int stat);
void remove_jobs(pid_t pid);

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
    signal(SIGTSTP, sigtstp_handler);
    
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
    pid_t pid;           /* Process id */
    int status;
    int isPipeCommand;
    
    strcpy(buf, cmdline);
    isPipeCommand = parseline(buf, argv);
    if (argv[0] == NULL)
        return;   /* Ignore empty lines */
    /* pipe parse error */
    if (isPipeCommand == -1) {
        printf("pipe parse error \n");
        return;
    }
    
    /* my pipe zone*/
    if (isPipeCommand){
        mypipe(argv, bg, cmdline);
        return;
    }
    else {
        if (bg){
            if (!builtin_command(argv)) { //quit -> exit(0), & -> ignore, other -> run
                if ((pid=Fork()) == 0) {
                    unblock_signals(); // receive signals in child process
                    if (execvp(argv[0], &argv[0]) < 0) {    //ex) /bin/ls ls -al &
                        printf("%s: Command not found.\n", argv[0]);
                        exit(0);
                    }
                }
                put_jobs(pid, cmdline, 1);
                printf("%d %s", pid, cmdline);
            }
        } else {
            if (!builtin_command(argv)) { //quit -> exit(0), & -> ignore, other -> run
                if ((pid=Fork()) == 0) {
                    unblock_signals(); // receive signals in child process
                    if (execvp(argv[0], &argv[0]) < 0) {    //ex) /bin/ls ls -al &
                        printf("%s: Command not found.\n", argv[0]);
                        exit(0);
                    }
                }
                fg_pid = pid; // is foreground
                strcpy(fg_cmdline, cmdline); // save foreground cmdline
                waitpid(pid, &status, WUNTRACED);
                fg_pid = 0; // after waitpid
        }
        }
    }
/* Parent waits for foreground job to terminate */
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
    if (!strcmp(argv[0], "jobs")){ // jobs
        show_jobs();
        return 1;
    }
    
    if (!strcmp(argv[0], "kill")){ // kill
        if (argv[1] == NULL) {
            return 0;
        }
        if (argv[1][0] == '%'){
            int idx = argv[1][1] - '0' - 1; // char -> int & get jobs idx
            if(jobs[idx].pid != 0){
                kill(jobs[idx].pid, SIGINT);
                return 1;
            }else{
                printf("No Such Job\n");
            }
        }
    }
    
    if (!strcmp(argv[0], "bg")){ // bg : Change a stopped background job to a running background job.
        if (argv[1] == NULL) {
            return 0;
        }
        if (argv[1][0] == '%'){
            int idx = argv[1][1] - '0' - 1; // char -> int & get jobs idx
            if(jobs[idx].pid != 0){
                kill(jobs[idx].pid, SIGCONT);
                jobs[idx].status = 1;
                return 1;
            }else{
                printf("No Such Job\n");
                
            }
        }
    }
    
    if (!strcmp(argv[0], "fg")){ // fg : Change a stopped or running a background job to a running in the foreground.
        if (argv[1] == NULL) {
            return 0;
        }
        if (argv[1][0] == '%'){
            int idx = argv[1][1] - '0' - 1; // char -> int & get jobs idx
            if(jobs[idx].pid != 0){
                fg_pid = jobs[idx].pid;
                kill(fg_pid, SIGCONT);
                jobs[idx].status = 1;
                waitpid(fg_pid, NULL, WUNTRACED);
                fg_pid = 0;
                return 1;
//                    kill(jobs[idx].pid, SIGCONT);
//                    jobs[idx].status = 1;
//                    waitpid(jobs[idx].pid, NULL, WUNTRACED);
//                    return 1;
            }
            else{
                printf("No Such Job\n");
            }
        }
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
    int isPipeCommand;

    buf[strlen(buf)-1] = ' ';  /* Replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* Ignore leading spaces */
        buf++;
    
    /* if command near pipe, need to seperate those */
    int len = (int)strlen(buf);
    for(int i=1; i<len-1; i++){
        if (buf[i] == '|' && buf[i-1] != ' ') {
            for(int j = len-1; j >= i; j--) {
                buf[j+1] = buf[j];
            }
            buf[i] = ' ';
            len += 1;
        }
        if (buf[i] == '|' && buf[i+1] != ' ') {
            for(int j = len-1; j > i; j--) {
                buf[j+1] = buf[j];
            }
            buf[i+1] = ' ';
            len += 1;
        }
    }
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
    /* one should not be required to separate the ‘&’ from the command by a space */
    int lastChar = (int)strlen(argv[argc-1]) - 1;
    if(argv[argc-1][lastChar] == '&'){
        if(lastChar == 0) {
            argv[--argc] = NULL;
        } else {
            argv[argc-1][lastChar] = '\0';
        }
        bg = 1;
    } else {
        bg = 0;
    }
    
//    if ((bg = (*argv[argc-1] == '&')) != 0){ // background
//        argv[--argc] = NULL;
//        bg = 1;
//    } else {
//        bg = 0;
//    }
    
    /* check is this pipe command / check proper pipe command */
    isPipeCommand = checkPipeCommand(argv);

    return isPipeCommand;
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
    char msg[100];
    strcpy(msg, "receive SIGINT\n");
    write(STDOUT_FILENO, msg, strlen(msg));
    return;
}
void sigquit_handler(int signum){
    char msg[100];
    strcpy(msg, "receive SIGQUIT\n");
    write(STDOUT_FILENO, msg, strlen(msg));
    return;
}

void sigchld_handler(int signum){
    int status;
    pid_t pid;
    
    while((pid = waitpid(-1, &status, WNOHANG)) > 0){ // reaping child process
        if (WIFEXITED(status)) { // child terminated normally
            remove_jobs(pid);
            }
        else if (WIFSIGNALED(status)) { // child terminated abnormally
            //printf("Child process %d terminated abnormally\n", pid);
            remove_jobs(pid);
            }
        }
    if(pid < 0) { // waitpid fail
        if(errno != ECHILD){
            perror("waitpid failed");
            exit(1);
        }
    }
    
}
void sigtstp_handler(int signum){
    if(fg_pid > 0) {
        put_jobs(fg_pid, fg_cmdline, 2);
        kill(fg_pid, SIGSTOP);
    }
}



void mypipe(char** argv, int bg, char* command){

    if (!bg){
        int pipeCount = 0;
        int pipeLimit = 100; // limit pipe count
        int pipeIndex[pipeLimit];
        pipeIndex[0] = 0;
        for(int i=0; argv[i] != NULL; i++){
            if (!strcmp(argv[i], "|")){
                argv[i] = NULL;
                pipeIndex[1+pipeCount++] = i+1;
                if (pipeCount >= 100) {
                    printf("pipe should be under 100\n");
                    return;
                }
            }
        }
        
        pid_t pid[pipeLimit];
        int fd[pipeLimit][2];
        
        for(int i=0; i<=pipeCount; i++){
            pipe(fd[i]);
            if ((pid[i] = Fork()) == 0){ // first command
                if(i == 0){
                    close(fd[i][0]);
                    dup2(fd[i][1], 1);
                    close(fd[i][1]);
                } else if ( i == pipeCount){ // last command
                    close(fd[i-1][1]);
                    dup2(fd[i-1][0], 0);
                    close(fd[i-1][0]);
                    close(fd[i][0]);
                    close(fd[i][1]);
                } else{ // middle command
                    dup2(fd[i-1][0],0);
                    close(fd[i-1][1]);
                    close(fd[i-1][0]);
                    dup2(fd[i][1],1);
                    close(fd[i][0]);
                    close(fd[i][1]);
                }
                if (execvp(argv[pipeIndex[i]], &argv[pipeIndex[i]]) < 0) {    //ex) /bin/ls ls -al &
                    unblock_signals(); // receive signals in child process
                    printf("%s: Command not found.\n", argv[pipeIndex[i]]);
                    exit(0);
                }
            }
            if(i > 0) {
                close(fd[i-1][0]);
                close(fd[i-1][1]);
            }
            if(i == pipeCount) {
                close(fd[i][0]);
                close(fd[i][1]);
            }
            fg_pid = pid[i]; // is foreground
            strcpy(fg_cmdline, command); // save foreground cmdline
            waitpid(pid[i], NULL, WUNTRACED);
            fg_pid = 0; // after waitpid
        }
    }
    else{
        int pipeCount = 0;
        int pipeLimit = 100; // limit pipe count
        int pipeIndex[pipeLimit];
        pipeIndex[0] = 0;
        for(int i=0; argv[i] != NULL; i++){
            if (!strcmp(argv[i], "|")){
                argv[i] = NULL;
                pipeIndex[1+pipeCount++] = i+1;
                if (pipeCount >= 100) {
                    printf("pipe should be under 100\n");
                    return;
                }
            }
        }
        
        pid_t pid[pipeLimit];
        int fd[pipeLimit][2];
        
        for(int i=0; i<=pipeCount; i++){
            pipe(fd[i]);
            if ((pid[i] = Fork()) == 0){ // first command
                if(i == 0){
                    close(fd[i][0]);
                    dup2(fd[i][1], 1);
                    close(fd[i][1]);
                } else if ( i == pipeCount){ // last command
                    close(fd[i-1][1]);
                    dup2(fd[i-1][0], 0);
                    close(fd[i-1][0]);
                    close(fd[i][0]);
                    close(fd[i][1]);
                } else{ // middle command
                    dup2(fd[i-1][0],0);
                    close(fd[i-1][1]);
                    close(fd[i-1][0]);
                    dup2(fd[i][1],1);
                    close(fd[i][0]);
                    close(fd[i][1]);
                }
                if (execvp(argv[pipeIndex[i]], &argv[pipeIndex[i]]) < 0) {    //ex) /bin/ls ls -al &
                    unblock_signals(); // receive signals in child process
                    printf("%s: Command not found.\n", argv[pipeIndex[i]]);
                    exit(0);
                }
            }
            if(i > 0) {
                close(fd[i-1][0]);
                close(fd[i-1][1]);
            }
            if(i == pipeCount) {
                close(fd[i][0]);
                close(fd[i][1]);
            }
            printf("%d ", pid[i]);
        }
        printf("%s\n", command);
    }
        
    return;
}

int checkPipeCommand(char** argv){
    int check = 0; // check is pipe command
    int argcount = 0; // args count
    for(int i=0; argv[i]!=NULL; i++) {
        argcount += 1;
        if(!strcmp(argv[i], "|")){
            check = 1;
        }
    }
    if (check == 0) { // no pipe command
        return 0;
    }
    /* check pipe continuous */
    check = 0;
    for(int i=0; argv[i]!=NULL; i++){
        if(!strcmp(argv[i], "|") && check == 0) {
            check = 1;
            continue;
        }
        if(strcmp(argv[i], "|") != 0 && check == 1) {
            check = 0;
            continue;
        }
        if(!strcmp(argv[i], "|") && check == 1) {
            return -1;
        }
    }
    /* check last arg is pipe */
    check = 0;
    if (!strcmp(argv[argcount-1], "|") || !strcmp(argv[0], "|")){
        return -1;
    }
    
    return 1; // yes pipe command
}

void unblock_signals(void) {
    sigset_t mask;
    sigemptyset(&mask);          // initialize mask
    sigaddset(&mask, SIGINT);    // remove SIGINT block
    sigaddset(&mask, SIGQUIT);   // remove SIGQUIT block
    sigaddset(&mask, SIGTSTP);   // remove SIGTSTP block
    sigprocmask(SIG_UNBLOCK, &mask, NULL);  // remove signals block
    return;
    
}



void show_jobs(void) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].pid != 0) {
            printf("[%d] %d ", i+1, jobs[i].pid);
            if (jobs[i].status == 1) {
                printf("Running ");
            } else if (jobs[i].status == 2) {
                printf("Suspended ");
            }
            printf("%s", jobs[i].command);
        }
    }
}

void put_jobs(pid_t pid, char* command, int stat){
    for(int i=0; i<num_jobs; i++){
        if(jobs[i].pid == pid) {
            jobs[i].status = stat;
            return;
        }
    }
    jobs[num_jobs].pid = pid;
    strcpy(jobs[num_jobs].command, command);
    jobs[num_jobs].status = stat;
    num_jobs++;
}

void remove_jobs(pid_t pid){
    for(int i=0; i<MAX_JOBS; i++){
        if(jobs[i].pid == pid) {
            jobs[i].pid = 0;
            jobs[i].command[0] = '\0';
            jobs[i].status = 0;
            break;
        }
    }
}
