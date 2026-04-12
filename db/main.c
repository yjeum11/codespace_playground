#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ptrace.h>
#include <linux/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>

#define MAXLINE 256
#define MAXTOKS 64

int is_running = 0;
int should_stop = 0;

int main(int argc, char **argv) {

    if (argc == 0) {
        printf("Please provide a file to debug\n");
        exit(1);
    }

    char *target = argv[1];

    pid_t child_pid = 0;

    do {
        if (is_running && should_stop) {
            int status;
            wait(&status);
            if (WIFSTOPPED(status)) {
                printf("process stopped\n");
            }
            should_stop = 0;
        }


        char command[MAXLINE];
        printf("(debug) ");
        fflush(stdout);
        if (fgets(command, MAXLINE, stdin) == NULL) {
            printf("Ending session..\n");
            if (is_running)
                kill(child_pid, SIGKILL);
            exit(0);
        }
        // printf("command: %s", command);

        int numtoks;
        char *str;
        char *tokens[64];
        for (numtoks = 0, str = command; ; numtoks++, str = NULL) {
            char *token = strtok(str, " \n");
            if (token == NULL) {
                break;
            }
            tokens[numtoks] = token;
        }
        if (numtoks == 0) continue;
        if (0 == strcmp(tokens[0], "r")) {
            if ((child_pid = fork()) == 0) {
                ptrace(PTRACE_TRACEME, 0, 0, NULL);
                execlp(target, target, NULL);
            }
            is_running = 1;
            // TODO: shouldn't stop after we implement breakpoints
            should_stop = 1;
            ptrace(PTRACE_SETOPTIONS, child_pid, NULL, PTRACE_O_TRACESYSGOOD | PTRACE_O_EXITKILL);
            printf("Launched process..\n");
        } else if (0 == strcmp(tokens[0], "n")) {
            if (!is_running) {
                printf("No process is running.\n");
                continue;
            }
            should_stop = 1;
            ptrace(PTRACE_SINGLESTEP, child_pid, 0, 0);
        } else if (0 == strcmp(tokens[0], "rip")) {
            struct user_regs_struct reg_buf;
            if (ptrace(PTRACE_GETREGS, child_pid, 0L, &reg_buf) == -1)
                printf("ptrace error\n");

            printf("At %%rip = 0x%x\n", reg_buf.rip);
            long inst = ptrace(PTRACE_PEEKDATA, child_pid, reg_buf.rip, NULL);
            printf("instruction is 0x%x\n", inst);
        }
    } while(1);
}
