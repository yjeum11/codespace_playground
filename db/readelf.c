#include <elf.h>
#include <stdio.h>
#include <string.h>

int validate (char *buffer) {
    Elf64_Ehdr *header = (Elf64_Ehdr *)buffer;
    if (memcmp(ELFMAG, header->e_ident, 4) != 0) {
        return -1;
    }
    printf("magic: %c%c%c%c\n", header->e_ident[0], header->e_ident[1], header->e_ident[2], header->e_ident[3]);
}