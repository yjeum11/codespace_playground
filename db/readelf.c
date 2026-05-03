#include <elf.h>
#include <stdio.h>
#include <string.h>

// typedef struct
// {
//   unsigned char	e_ident[EI_NIDENT];	/* Magic number and other info */
//   Elf64_Half	e_type;			/* Object file type */
//   Elf64_Half	e_machine;		/* Architecture */
//   Elf64_Word	e_version;		/* Object file version */
//   Elf64_Addr	e_entry;		/* Entry point virtual address */
//   Elf64_Off	e_phoff;		/* Program header table file offset */
//   Elf64_Off	e_shoff;		/* Section header table file offset */
//   Elf64_Word	e_flags;		/* Processor-specific flags */
//   Elf64_Half	e_ehsize;		/* ELF header size in bytes */
//   Elf64_Half	e_phentsize;		/* Program header table entry size */
//   Elf64_Half	e_phnum;		/* Program header table entry count */
//   Elf64_Half	e_shentsize;		/* Section header table entry size */
//   Elf64_Half	e_shnum;		/* Section header table entry count */
//   Elf64_Half	e_shstrndx;		/* Section header string table index */
// } Elf64_Ehdr;

// typedef struct
// {
//   Elf64_Word	sh_name;		/* Section name (string tbl index) */
//   Elf64_Word	sh_type;		/* Section type */
//   Elf64_Xword	sh_flags;		/* Section flags */
//   Elf64_Addr	sh_addr;		/* Section virtual addr at execution */
//   Elf64_Off	sh_offset;		/* Section file offset */
//   Elf64_Xword	sh_size;		/* Section size in bytes */
//   Elf64_Word	sh_link;		/* Link to another section */
//   Elf64_Word	sh_info;		/* Additional section information */
//   Elf64_Xword	sh_addralign;		/* Section alignment */
//   Elf64_Xword	sh_entsize;		/* Entry size if section holds table */
// } Elf64_Shdr;

typedef struct {
    Elf64_Ehdr *header;
    Elf64_Shdr **sections;
} elf_t;

int validate (char *buffer) {
    Elf64_Ehdr *header = (Elf64_Ehdr *)buffer;
    if (memcmp(ELFMAG, header->e_ident, 4) != 0) {
        printf("Executable is not a valid ELF file.\n");
        return -1;
    }
    // printf("magic: %c%c%c%c\n", header->e_ident[0], header->e_ident[1], header->e_ident[2], header->e_ident[3]);
    // printf("section header table entry size: %ld, count: %ld, offset: %ld\n", header->e_shentsize, header->e_shnum, header->e_shoff);
    Elf64_Shdr *shtable = (Elf64_Shdr *) (buffer + (header->e_shoff));

    Elf64_Shdr *symtab_header = NULL;
    printf("sizeof(Shdr) = %d\n", sizeof(Elf64_Shdr));
    for (int i = 0; i < header->e_shnum; i++) {
        if (shtable[i].sh_type == SHT_SYMTAB) {
            symtab_header = &shtable[i];
        }
    }
    if (symtab_header == NULL) {
        printf("No symbol table found in executable.\n");
        return -1;
    }
    Elf64_Shdr *sh_strtab = shtable+(header->e_shstrndx);
    char *sh_string_table = buffer+sh_strtab->sh_offset;
    printf("%s\n", &sh_string_table[symtab_header->sh_name]);

    Elf64_Sym *symtab = (Elf64_Sym *)(buffer + symtab_header->sh_offset);
}
