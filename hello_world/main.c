int _start(void) {
    char msg[15] = "Hello, world!\n";
    asm volatile (
        "mov $0x1, %%rdi\n"
        "mov $0xe, %%rdx\n"
        "lea %0, %%rsi\n"
        "mov $0x1, %%rax\n"
        "syscall\n"
        "mov $0x0, %%rdi\n"
        "mov $0x3c, %%rax\n"
        "syscall"
        :
        : "m" (msg)
        : "%rdi", "%rdx", "%rsi", "%rax"
    );
    return 0;
}