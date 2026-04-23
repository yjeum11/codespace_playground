#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ptrace.h>
#include <linux/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <elf.h>
#include <errno.h>

#define MAXLINE 256
#define MAXTOKS 64

int is_running = 0;

typedef struct {
    int is_running;
    int in_breakpoint;
} app_state_t;

typedef struct {
    size_t address;
    int active;
    long shadowed;
} breakpoint_info_t;

int tokenize(char **output, char *input) {
    int numtoks;
    char *str;
    for (numtoks = 0, str = input; ; numtoks++, str = NULL) {
        char *token = strtok(str, " \n");
        if (token == NULL) {
            break;
        }
        output[numtoks] = token;
    }
    return numtoks;
}

void wait_for_child(int *status, pid_t child_pid, int *exiting_breakpoint, breakpoint_info_t bp) {
    wait(status);
    if (WIFEXITED(*status)) {
        printf("process exited\n");
        is_running = 0;
    }
    if (WIFSTOPPED(*status)) {
        printf("process stopped by signal %s\n", strsignal(WSTOPSIG(*status)));
        struct user_regs_struct reg_buf;

        if (ptrace(PTRACE_GETREGS, child_pid, 0L, &reg_buf) == -1)
            printf("ptrace error\n");
        
        // CPU increments rip by 1 byte after hitting interrupt
        long breakpoint = ptrace(PTRACE_PEEKDATA, child_pid, reg_buf.rip - 0x1, NULL);
        if ((breakpoint & 0xFF) == 0xCC) {
            printf("hit breakpoint at address %lx\n", bp.address);
            if (exiting_breakpoint != NULL) {
                *exiting_breakpoint = 1;
                reg_buf.rip -= 1;
                if (ptrace(PTRACE_SETREGS, child_pid, NULL, &reg_buf) == -1) {
                    printf("ptrace error 1\n");
                    if (errno == ESRCH) {
                        printf("esrch\n");
                    }
                }
                if (ptrace(PTRACE_POKEDATA, child_pid, reg_buf.rip, bp.shadowed) == -1) {
                    printf("ptrace error 2\n");
                    if (errno == ESRCH) {
                        printf("esrch\n");
                    }
                }
            }
        } else {
            printf("didn't hit breakpoint\n");
            if (exiting_breakpoint != NULL) {
                *exiting_breakpoint = 0;
            }
        }
    }
}

int main(int argc, char **argv) {
    if (argc == 0) {
        printf("Please provide a file to debug\n");
        exit(1);
    }

    char *target = argv[1];

    pid_t child_pid = 0;

    app_state_t prev_state, curr_state;
    prev_state.in_breakpoint = 0;
    prev_state.is_running = 0;
    curr_state.in_breakpoint = 0;
    curr_state.is_running = 0;

    // TODO: make this into some structure to support multiple breakpoints
    breakpoint_info_t bp;
    int exiting_breakpoint = 0;

    do {
        char command[MAXLINE];
        int status;
        printf("(debug) ");
        fflush(stdout);
        if (fgets(command, MAXLINE, stdin) == NULL) {
            printf("Ending session..\n");
            if (is_running)
                kill(child_pid, SIGKILL);
            exit(0);
        }
        // printf("command: %s", command);

        char *tokens[64];
        int numtoks = tokenize(tokens, command);
        if (numtoks == 0) continue;

        if (0 == strcmp(tokens[0], "r")) {
            if (is_running) {
                printf("target already running.\n");
                continue;
            }
            if ((child_pid = fork()) == 0) {
                ptrace(PTRACE_TRACEME, 0, 0, NULL);
                execlp(target, target, NULL);
            }
            is_running = 1;
            // TODO: shouldn't stop after we implement breakpoints
            ptrace(PTRACE_SETOPTIONS, child_pid, NULL, PTRACE_O_TRACESYSGOOD | PTRACE_O_EXITKILL);
            printf("Launched process..\n");
            wait_for_child(&status, child_pid, NULL, bp);
        } else if (0 == strcmp(tokens[0], "c")) { 
            if (!is_running) {
                printf("process is not running\n");
                continue;
            }
            if (exiting_breakpoint) {
                // reactivate breakpoint
                ptrace(PTRACE_SINGLESTEP, child_pid, 0, 0);
                wait_for_child(&status, child_pid, &exiting_breakpoint, bp);
                // insert INT3 instruction
                long inserting = bp.shadowed & (~0xffL) | 0xcc;
                ptrace(PTRACE_POKEDATA, child_pid, (void *)bp.address, inserting);
            }
            ptrace(PTRACE_CONT, child_pid, NULL, NULL);
            // checks if we hit a breakpoint.
            // if so, we point rip at the breakpoint instruction and restore the original instruction
            wait_for_child(&status, child_pid, &exiting_breakpoint, bp);
        } else if (0 == strcmp(tokens[0], "n")) {
            if (!is_running) {
                printf("No process is running.\n");
                continue;
            }
            if (exiting_breakpoint) {
                printf("n snr, exiting_breakpoint = %d\n", exiting_breakpoint);
                // guaranteed atp to be pointing at the address of the instruction
                struct user_regs_struct temp;
                ptrace(PTRACE_GETREGS, child_pid, 0, &temp);
                printf("current rip = %lx\n", temp.rip);
                long inst = ptrace(PTRACE_PEEKDATA, child_pid, temp.rip, NULL);
                printf("instruction is 0x%lx\n", inst);
                ptrace(PTRACE_SINGLESTEP, child_pid, 0, 0);
                wait_for_child(&status, child_pid, &exiting_breakpoint, bp);
                // insert INT3 instruction
                long inserting = bp.shadowed & (~0xffL) | 0xcc;
                ptrace(PTRACE_POKEDATA, child_pid, (void *)bp.address, inserting);
            } else {
                ptrace(PTRACE_SINGLESTEP, child_pid, 0, 0);
                wait_for_child(&status, child_pid, &exiting_breakpoint, bp);
            }
        } else if (0 == strcmp(tokens[0], "rip")) {
            struct user_regs_struct reg_buf;
            if (ptrace(PTRACE_GETREGS, child_pid, 0L, &reg_buf) == -1)
                printf("ptrace error\n");

            printf("At %%rip = 0x%llx\n", reg_buf.rip);
            long inst = ptrace(PTRACE_PEEKDATA, child_pid, reg_buf.rip, NULL);
            printf("instruction is 0x%lx\n", inst);
        } else if (0 == strcmp(tokens[0], "b")) {
            breakpoint_info_t new;
            new.active = 1;
            new.address = strtol(tokens[1], NULL, 16);
            new.shadowed = ptrace(PTRACE_PEEKDATA, child_pid, (void *)new.address, NULL);
            printf("shadowing %lx\n", new.shadowed);
            bp = new;

            // insert INT3 instruction
            long inserting = new.shadowed & (~0xffL) | 0xcc;
            ptrace(PTRACE_POKEDATA, child_pid, (void *)new.address, inserting);
        }
    } while(1);
}
