#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 500

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

#include "readelf.h"

#define MAXLINE 256
#define MAXTOKS 64

typedef struct {
    size_t address;
    int active;
    uint8_t shadowed;
} breakpoint_info_t;

typedef struct {
    int is_running;
    int in_breakpoint;
    breakpoint_info_t *breakpoint;
} app_state_t;

typedef struct bp_node {
    breakpoint_info_t *b;
    int index;
    struct bp_node *next;
} bp_node_t;

typedef struct bp_list {
    bp_node_t *head;
    int next_index;
} bp_list_t;

bp_list_t *new_bp_list() {
    bp_list_t *r = malloc(sizeof(bp_list_t));
    r->head = NULL;
    r->next_index = 0;
    return r;
}

void free_bp_list(bp_list_t *list) {
    if (list == NULL) return;
    bp_node_t *n = list->head;
    while (n != NULL) {
        bp_node_t *temp = n;
        n = n->next;
        free(temp->b);
        free(temp);
    }
}

void add_breakpoint(bp_list_t *list, breakpoint_info_t *data) {
    bp_node_t *n = malloc(sizeof(bp_node_t));
    n->b = data;
    n->next = list->head;
    n->index = list->next_index++;
    list->head = n;
}

void display_breakpoints(bp_list_t *list) {
    for (bp_node_t *p = list->head; p != NULL; p = p->next) {
        printf("breakpoint at address %lx\n", p->b->address);
    }
}

long replace_lsb(long input, uint8_t replacement) {
    return (input & (~0xffL)) | replacement;
}

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

void remove_breakpoints(pid_t child_pid, bp_list_t *bp_list) {
    for (bp_node_t *p = bp_list->head; p != NULL; p = p->next) {
        breakpoint_info_t *b = p->b;
        if (b->active) {
            // printf("removing %lx\n", b->address);
            long old_data = ptrace(PTRACE_PEEKDATA, child_pid, b->address, NULL);
            long new_data = replace_lsb(old_data, b->shadowed);
            b->active = 0;
            ptrace(PTRACE_POKEDATA, child_pid, b->address, new_data);
        }
    }
}

void insert_breakpoints(pid_t child_pid, bp_list_t *bp_list) {
    for (bp_node_t *p = bp_list->head; p != NULL; p = p->next) {
        breakpoint_info_t *b = p->b;
        if (!(b->active)) {
            // printf("inserting %lx\n", b->address);
            long old_data = ptrace(PTRACE_PEEKDATA, child_pid, b->address, NULL);
            long new_data = replace_lsb(old_data, 0xcc);
            b->active = 1;
            if (ptrace(PTRACE_POKEDATA, child_pid, b->address, new_data) == -1){
                printf("ptrace error\n");
                if (errno == ESRCH) {
                    printf("not stopped\n");
                }
            }
        }
    }
}


void wait_for_child(int *status, pid_t child_pid, app_state_t *state, bp_list_t *bp_list) {
    wait(status);
    if (WIFEXITED(*status)) {
        printf("process exited\n");
        state->is_running = 0;
    }
    if (WIFSTOPPED(*status)) {
        if (WSTOPSIG(*status) != SIGTRAP) {
            
        }
        struct user_regs_struct reg_buf;

        if (ptrace(PTRACE_GETREGS, child_pid, 0L, &reg_buf) == -1)
            printf("ptrace error\n");
        
        // CPU increments rip by 1 byte after hitting interrupt
        // after thebreakpoint has been hit, we want to rewind the state of the cpu
        // such that it's right before hitting the breakpoint.
        // so the next 'n' will execute the instruction that the breakpoint shadowed
        long breakpoint = ptrace(PTRACE_PEEKDATA, child_pid, reg_buf.rip - 0x1, NULL);
        if ((breakpoint & 0xFF) == 0xCC) {
            // printf("hit breakpoint at address %llx\n", reg_buf.rip - 0x1);
            bp_node_t *p = bp_list->head;
            breakpoint_info_t *hit_breakpoint = NULL;
            while (p != NULL) {
                if (p->b->address == reg_buf.rip - 0x1) {
                    hit_breakpoint = p->b;
                    break;
                }
                p = p->next;
            }
            
            state->in_breakpoint = 1;
            state->breakpoint = hit_breakpoint;
            reg_buf.rip -= 1;
            if (ptrace(PTRACE_SETREGS, child_pid, NULL, &reg_buf) == -1) {
                printf("ptrace error 1\n");
            }

            long old_data = ptrace(PTRACE_PEEKDATA, child_pid, reg_buf.rip, NULL);
            long new_data = replace_lsb(old_data, hit_breakpoint->shadowed);
            if (ptrace(PTRACE_POKEDATA, child_pid, reg_buf.rip, new_data) == -1) {
                printf("ptrace error 2\n");
            }
        } else {
            state->in_breakpoint = 0;
            state->breakpoint = NULL;
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
    prev_state.breakpoint = NULL;
    curr_state.in_breakpoint = 0;
    curr_state.is_running = 0;
    curr_state.breakpoint = NULL;

    FILE *target_fd = fopen(target, "rb");
    if (target_fd == NULL) {
        printf("Target file %s doesn't exist\n", target);
        exit(1);
    }

    fseek(target_fd, 0, SEEK_END);
    long target_size = ftell(target_fd);
    rewind(target_fd);

    char *target_buf = malloc(target_size);
    fread(target_buf, target_size, 1, target_fd);

    fclose(target_fd);

    if (validate(target_buf) == -1) {
        printf("Error while processing executable. exiting..");
        exit(1);
    }

    bp_list_t *breakpoints = new_bp_list();

    // At the beginning of the loop, the child process should be in ptrace-stop
    do {
        if (curr_state.is_running) {
            struct user_regs_struct reg_buf;
            ptrace(PTRACE_GETREGS, child_pid, 0L, &reg_buf);
            long current_rip = reg_buf.rip;

            for (bp_node_t *p = breakpoints->head; p != NULL; p = p->next) {
                if (p->b->active && p->b->address == current_rip) {
                    printf("Breakpoint %d: %lx\n\n", p->index, p->b->address);
                }
            }
        }

        char command[MAXLINE];
        int status;
        printf("(debug) ");
        fflush(stdout);
        if (fgets(command, MAXLINE, stdin) == NULL) {
            printf("Ending session..\n");
            if (curr_state.is_running)
                kill(child_pid, SIGKILL);
            free_bp_list(breakpoints);
            exit(0);
        }

        char *tokens[64];
        int numtoks = tokenize(tokens, command);
        if (numtoks == 0) continue;

        if (0 == strcmp(tokens[0], "r")) {
            if (curr_state.is_running) {
                printf("target already running.\n");
                continue;
            }
            if ((child_pid = fork()) == 0) {
                ptrace(PTRACE_TRACEME, 0, 0, NULL);
                execlp(target, target, NULL);
            }
            curr_state.is_running = 1;
            ptrace(PTRACE_SETOPTIONS, child_pid, NULL, PTRACE_O_TRACESYSGOOD | PTRACE_O_EXITKILL);
            printf("Launched process..\n");
            wait_for_child(&status, child_pid, &curr_state, breakpoints);
            insert_breakpoints(child_pid, breakpoints);
            ptrace(PTRACE_CONT, child_pid, NULL, NULL);
            wait_for_child(&status, child_pid, &curr_state, breakpoints);
        } else if (0 == strcmp(tokens[0], "c")) { 
            if (!curr_state.is_running) {
                printf("process is not running\n");
                continue;
            }
            if (prev_state.in_breakpoint) {
                // reactivate breakpoint
                // printf("reactivate breakpoint\n");
                ptrace(PTRACE_SINGLESTEP, child_pid, 0, 0);
                wait_for_child(&status, child_pid, &curr_state, breakpoints);
                // insert INT3 instruction
                long old_data = ptrace(PTRACE_PEEKDATA, child_pid, (void *)prev_state.breakpoint->address, NULL);
                long inserting = replace_lsb(old_data, 0xcc);
                ptrace(PTRACE_POKEDATA, child_pid, (void *)prev_state.breakpoint->address, inserting);
            }
            ptrace(PTRACE_CONT, child_pid, NULL, NULL);
            // checks if we hit a breakpoint.
            // if so, we point rip at the breakpoint instruction and restore the original instruction
            wait_for_child(&status, child_pid, &curr_state, breakpoints);
        } else if (0 == strcmp(tokens[0], "n")) {
            if (!curr_state.is_running) {
                printf("No process is running.\n");
                continue;
            }
            if (prev_state.in_breakpoint) {
                ptrace(PTRACE_SINGLESTEP, child_pid, 0, 0);
                wait_for_child(&status, child_pid, &curr_state, breakpoints);
                // insert INT3 instruction
                long old_data = ptrace(PTRACE_PEEKDATA, child_pid, (void *)prev_state.breakpoint->address, NULL);
                long inserting = replace_lsb(old_data, 0xcc);
                ptrace(PTRACE_POKEDATA, child_pid, (void *)prev_state.breakpoint->address, inserting);
            } else {
                struct user_regs_struct reg_buf;
                ptrace(PTRACE_GETREGS, child_pid, NULL, &reg_buf);
                uint8_t curr_byte = ptrace(PTRACE_PEEKDATA, child_pid, reg_buf.rip, NULL) & 0xff;
                if (curr_byte == 0xcc) {
                    // printf("activating special case\n");
                    remove_breakpoints(child_pid, breakpoints);
                }
                // if current instruction is a breakpoint
                // disable breakpoints
                ptrace(PTRACE_SINGLESTEP, child_pid, 0, 0);
                wait_for_child(&status, child_pid, &curr_state, breakpoints);
                if (curr_byte == 0xcc) {
                    // printf("inserting breakpoints back\n");
                    insert_breakpoints(child_pid, breakpoints);
                }
            }
        } else if (0 == strcmp(tokens[0], "rip")) {
            struct user_regs_struct reg_buf;
            if (ptrace(PTRACE_GETREGS, child_pid, 0L, &reg_buf) == -1)
                printf("ptrace error\n");

            printf("At %%rip = 0x%llx\n", reg_buf.rip);
            long inst = ptrace(PTRACE_PEEKDATA, child_pid, reg_buf.rip, NULL);
            printf("instruction is 0x%lx\n", inst);
        } else if (0 == strcmp(tokens[0], "b")) {
            breakpoint_info_t *new = malloc(sizeof(breakpoint_info_t));
            new->active = curr_state.is_running ? 1 : 0;
            new->address = strtol(tokens[1], NULL, 16);
            new->shadowed = ptrace(PTRACE_PEEKDATA, child_pid, (void *)new->address, NULL) && 0xFF;

            // TODO: add bp to some structure
            add_breakpoint(breakpoints, new);
            printf("New breakpoint at address %lx\n", new->address);

            if (curr_state.is_running) {
                // insert INT3 instruction
                long old_data = ptrace(PTRACE_PEEKDATA, child_pid, (void *)new->address, NULL);
                long inserting = replace_lsb(old_data, 0xcc);
                ptrace(PTRACE_POKEDATA, child_pid, (void *)new->address, inserting);
            }
        } else if (0 == strcmp(tokens[0], "f")) {
            // x86-64 stack frame:
            // 

            printf("Current stack frame: \n");
            struct user_regs_struct reg_buf;
            ptrace(PTRACE_GETREGS, child_pid, NULL, &reg_buf);
            unsigned long long rsp = reg_buf.rsp;
            unsigned long long rbp = reg_buf.rbp;
        }else if (0 == strcmp(tokens[0], "bt")) {

        } else if (0 == strcmp(tokens[0], "i")) {
            if (tokens[1] && 0 == strcmp(tokens[1], "r")) {
                struct user_regs_struct reg_buf;
                ptrace(PTRACE_GETREGS, child_pid, NULL, &reg_buf);
                printf("rax: %llx\n", reg_buf.rax);
            } else {
                display_breakpoints(breakpoints);
            }
        } else if (0 == strcmp(tokens[0], "p")) {
            size_t read_addr = strtol(tokens[1], NULL, 16);
            long data = ptrace(PTRACE_PEEKDATA, child_pid, read_addr, NULL);
            printf("Address %lx holds \n", read_addr);
            printf("%lx\n", data);
        }
        prev_state = curr_state;
    } while(1);
}
