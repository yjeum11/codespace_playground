#ifndef READELF_H
#define READELF_H

typedef struct elf_local elf_t;

elf_t *new_elf (char *buffer);
void free_elf (elf_t *elf);
char *symbol_name(elf_t *elf, int symtab_idx);
char *section_name(elf_t *elf, int section_idx);
void *find_section(elf_t *elf, char *name);
size_t function_to_address(elf_t *elf, char *name);

#endif