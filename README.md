# Playground

## Debugger

I wrote a debugger which works with the Linux `ptrace` API to single-step and set breakpoints.
This will work on x86-64 statically linked executables since I didn't want to figure out how dynamic linking works.
Currently only software breakpoints are supported which work by overwriting the instruction in the running process's address space with the x86 INT3 instruction, which triggers a software trap.
I tried to mimic `gdb`'s behavior on strange edge cases (i.e. single-stepping onto a breakpoint)

### Usage
```
./ydb <target>
```

### Command reference

| Command | Function |
|:---:|:---|
| `r` | run process |
| `c` | continue execution |
| `n` | next instruction (single-step) |
| `b <address>` | set breakpoint at `<address>` |
| `rip` | print current %rip and instruction |
| `p <address>` | print memory contents at `<address>` |

### Future work

- Adding ELF file parsing to allow line numbers when setting breakpoints
- Stack backtrace

## Dithering

![golden](dithering/david.png)

Input

![output](dithering/output_david.png)

Output of 16x16 dithering.