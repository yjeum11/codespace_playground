#include <elf.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "readelf.h"

struct elf_local {
    Elf64_Ehdr *header;
    Elf64_Shdr *sh_table;
    char *sh_strtab;
    Elf64_Shdr *sym_table_header;
    Elf64_Sym *sym_table;
    char *sym_strtab;
    char *buffer;
};

elf_t *new_elf (char *buffer) {
    Elf64_Ehdr *header = (Elf64_Ehdr *)buffer;
    if (memcmp(ELFMAG, header->e_ident, 4) != 0) {
        printf("Executable is not a valid ELF file.\n");
        return NULL;
    }
    elf_t *new = malloc(sizeof(elf_t));
    new->buffer = buffer;
    new->header = header;
    new->sh_table = (Elf64_Shdr *) (buffer + (header->e_shoff));
    Elf64_Shdr *sh_strtab_header = new->sh_table+(header->e_shstrndx);
    new->sh_strtab = buffer+sh_strtab_header->sh_offset;

    Elf64_Shdr *symtab_header = NULL;
    for (int i = 0; i < header->e_shnum; i++) {
        if (new->sh_table[i].sh_type == SHT_SYMTAB) {
            symtab_header = &new->sh_table[i];
        }
    }
    new->sym_strtab = find_section(new, ".strtab");
    if (symtab_header == NULL || new->sym_strtab == NULL) {
        printf("No symbol table found in executable.\n");
        return NULL;
    }
    new->sym_table_header = symtab_header;

    new->sym_table = (Elf64_Sym *)(buffer + symtab_header->sh_offset);

    return new;
}

void free_elf (elf_t *elf) {
    free(elf->buffer);
    free(elf);
}

char *symbol_name(elf_t *elf, int symtab_idx) {
    return elf->sym_strtab + elf->sym_table[symtab_idx].st_name;
}

char *section_name(elf_t *elf, int section_idx) {
    return elf->sh_strtab + elf->sh_table[section_idx].sh_name;
}

void *find_section(elf_t *elf, char *name) {
    for (int i = 0; i < elf->header->e_shnum; i++) {
        Elf64_Shdr curr_section = elf->sh_table[i];
        if (strcmp(section_name(elf, i), name) == 0) {
            return (void *)(elf->buffer + curr_section.sh_offset);
        }
    }
    return NULL;
}

size_t function_to_address(elf_t *elf, char *name) {
    int num_syms = elf->sym_table_header->sh_size / elf->sym_table_header->sh_entsize;
    for (int i = 0; i < num_syms; i++) {
        if (0 == strcmp(symbol_name(elf, i), name)) {
            Elf64_Sym fn = elf->sym_table[i];
            if (ELF64_ST_TYPE(fn.st_info) == STT_FUNC) {
                return fn.st_value;
            }
        }
    }
    return 0;
}