#define _POSIX_C_SOURCE 199309L

#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

volatile sig_atomic_t flag = 0;

void sigtrap_handler(int signum) {

    flag = 1;
}

int main() {
    struct sigaction install = { 0 };
    install.sa_handler = *sigtrap_handler;
    sigaction(SIGCHLD, &install, NULL);

    kill(getpid(), SIGCHLD);
    if (flag == 1) return 0;
    else return -1;
}