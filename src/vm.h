#ifndef VM_H
#define VM_H

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <sys/types.h>
/* unix only */
#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/termios.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

namespace VM {
using namespace std;

#define MEMORY_MAX (1 << 16)
#define PC_START 0x3000

enum registers {
  R_R0 = 0,
  R_R1,
  R_R2,
  R_R3,
  R_R4,
  R_R5,
  R_R6,
  R_R7,
  R_PC, /* program counter */
  R_COND,
  R_COUNT
};

enum instructions {
  OP_BR = 0, /* branch */
  OP_ADD,    /* add  */
  OP_LD,     /* load */
  OP_ST,     /* store */
  OP_JSR,    /* jump register */
  OP_AND,    /* bitwise and */
  OP_LDR,    /* load register */
  OP_STR,    /* store register */
  OP_RTI,    /* unused */
  OP_NOT,    /* bitwise not */
  OP_LDI,    /* load indirect */
  OP_STI,    /* store indirect */
  OP_JMP,    /* jump */
  OP_RES,    /* reserved (unused) */
  OP_LEA,    /* load effective address */
  OP_TRAP    /* execute trap */
};

enum cond_flags {
  FL_POS = 1 << 0, /* P */
  FL_ZRO = 1 << 1, /* Z */
  FL_NEG = 1 << 2, /* N */
};

enum traps {
  TRAP_GETC = 0x20,  /* get character from keyboard */
  TRAP_OUT = 0x21,   /* output a character */
  TRAP_PUTS = 0x22,  /* output a word string */
  TRAP_IN = 0x23,    /* input a string */
  TRAP_PUTSP = 0x24, /* output a byte string */
  TRAP_HALT = 0x25   /* halt the program */
};
enum kb {
  MR_KBSR = 0xFE00, /* keyboard status */
  MR_KBDR = 0xFE02  /* keyboard data */
};

class vm {
private:
  uint16_t memory[MEMORY_MAX];
  uint16_t regs[R_COUNT];
  bool run = 1;

  uint16_t sign_extend(uint16_t x, int bit_count) {
    if ((x >> (bit_count - 1)) & 1) {
      x |= (0xFFFF << bit_count);
    }
    return x;
  }

  void update_flags(uint16_t r) {
    if (regs[r] == 0) {
      regs[R_COND] = FL_ZRO;
    } else if (regs[r] >> 15) /* a 1 in the left-most bit indicates negative */
    {
      regs[R_COND] = FL_NEG;
    } else {
      regs[R_COND] = FL_POS;
    }
  }
  void mem_write(uint16_t address, uint16_t val) { memory[address] = val; }

  uint16_t mem_read(uint16_t address) {
    if (address == MR_KBSR) {
      if (check_key()) {
        memory[MR_KBSR] = (1 << 15);
        memory[MR_KBDR] = getchar();
      } else {
        memory[MR_KBSR] = 0;
      }
    }
    return memory[address];
  }

  uint16_t check_key() {
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    return select(1, &readfds, NULL, NULL, &timeout) != 0;
  }
  uint16_t swap16(uint16_t x) { return (x << 8) | (x >> 8); }

  void BR(uint16_t instr) {
    uint16_t offset = sign_extend(instr & 0x1ff, 9);
    uint16_t cond = (instr >> 9) & 0x7;
    if (cond & regs[R_COND]) {
      regs[R_PC] += offset;
    }
  }
  void ADD(uint16_t instr) {
    uint16_t dr = (instr >> 9) & 0x7;
    uint16_t sr1 = (instr >> 6) & 0x7;
    uint16_t imm_flag = (instr >> 5) & 0x1;

    if (imm_flag) {
      uint16_t imm5 = sign_extend(instr & 0x1f, 5);
      regs[dr] = regs[sr1] + imm5;
    } else {
      uint16_t sr2 = instr & 0x7;
      regs[dr] = regs[sr1] + regs[sr2];
    }

    update_flags(dr);
  }
  void LD(uint16_t instr) {
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t pc_offset = sign_extend(instr & 0x1ff, 9);
    regs[r0] = mem_read(regs[R_PC] + pc_offset);
    update_flags(r0);
  }
  void ST(uint16_t instr) {
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
    mem_write(regs[R_PC] + pc_offset, regs[r0]);
  }
  void JSR(uint16_t instr) {
    uint16_t r1 = (instr >> 6) & 0x7;
    uint16_t long_pc_offset = sign_extend(instr & 0x7ff, 11);
    uint16_t long_flag = (instr >> 11) & 1;

    regs[R_R7] = regs[R_PC];
    if (long_flag) {
      regs[R_PC] += long_pc_offset; /* JSR */
    } else {
      regs[R_PC] = regs[r1]; /* JSRR */
    }
  }
  void AND(uint16_t instr) {
    uint16_t dr = (instr >> 9) & 0x7;
    uint16_t sr1 = (instr >> 6) & 0x7;
    uint16_t imm_flag = (instr >> 5) & 0x1;

    if (imm_flag) {
      uint16_t imm5 = sign_extend(instr & 0x1f, 5);
      regs[dr] = regs[sr1] & imm5;
    } else {
      uint16_t sr2 = instr & 0x7;
      regs[dr] = regs[sr1] & regs[sr2];
    }

    update_flags(dr);
  }
  void LDR(uint16_t instr) {
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t r1 = (instr >> 6) & 0x7;
    uint16_t offset = sign_extend(instr & 0x3F, 6);
    regs[r0] = mem_read(regs[r1] + offset);
    update_flags(r0);
  }
  void STR(uint16_t instr) {
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t r1 = (instr >> 6) & 0x7;
    uint16_t offset = sign_extend(instr & 0x3F, 6);
    mem_write(regs[r1] + offset, regs[r0]);
  }
  void NOT(uint16_t instr) {
    uint16_t dr = (instr >> 9) & 0x7;
    uint16_t sr = (instr >> 6) & 0x7;

    regs[dr] = ~regs[sr];

    update_flags(dr);
  }
  void LDI(uint16_t instr) {
    uint16_t dr = (instr >> 9) & 0x7;
    uint16_t pcoffset = sign_extend(instr & 0x1ff, 9);
    regs[dr] = mem_read(mem_read(regs[R_PC] + pcoffset));
    update_flags(dr);
  }
  void STI(uint16_t instr) {
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
    mem_write(mem_read(regs[R_PC] + pc_offset), regs[r0]);
  }
  void JMP(uint16_t instr) {
    uint16_t br = (instr >> 6) & 0x7;
    regs[R_PC] = regs[br];
  }
  void LEA(uint16_t instr) {
    uint16_t r0 = (instr >> 9) & 0x7;
    uint16_t pc_offset = sign_extend(instr & 0x1FF, 9);
    regs[r0] = regs[R_PC] + pc_offset;
    update_flags(r0);
  }
  void TRAP(uint16_t instr) {
    switch (instr & 0xFF) {
    case TRAP_GETC: {
      regs[R_R0] = (uint16_t)getchar();
      break;
    }
    case TRAP_OUT: {
      putc((char)regs[R_R0], stdout);
      fflush(stdout);
      break;
    }
    case TRAP_PUTS: {
      uint16_t *c = memory + regs[R_R0];
      while (*c) {
        putc((char)*c, stdout);
        ++c;
      }
      fflush(stdout);
      break;
    }
    case TRAP_IN: {
      printf("Enter a character: ");
      char c = getchar();
      putc(c, stdout);
      fflush(stdout);
      regs[R_R0] = (uint16_t)c;
      update_flags(R_R0);
      break;
    }
    case TRAP_PUTSP: {
      /* one char per byte (two bytes per word)
         here we need to swap back to
         big endian format */
      uint16_t *c = memory + regs[R_R0];
      while (*c) {
        char char1 = (*c) & 0xFF;
        putc(char1, stdout);
        char char2 = (*c) >> 8;
        if (char2)
          putc(char2, stdout);
        ++c;
      }
      fflush(stdout);
      break;
    }
    case TRAP_HALT: {
      puts("HALT");
      fflush(stdout);
      run = 0;
      break;
    }
    }
  }

public:
  void read_image_file(FILE *file) {
    /* the origin tells us where in memory to place the image */
    uint16_t origin;
    fread(&origin, sizeof(origin), 1, file);
    origin = swap16(origin);

    /* we know the maximum file size so we only need one fread */
    uint16_t max_read = UINT16_MAX - origin;
    uint16_t *p = memory + origin;
    size_t read = fread(p, sizeof(uint16_t), max_read, file);

    /* swap to little endian */
    while (read-- > 0) {
      *p = swap16(*p);
      ++p;
    }
  }
  int read_image(const char *image_path) {
    FILE *file = fopen(image_path, "rb");
    if (!file) {
      return 0;
    };
    read_image_file(file);
    fclose(file);
    return 1;
  }
  void execute() {
    regs[R_COND] = FL_ZRO;
    regs[R_PC] = PC_START;

    while (run) {
      uint16_t instr = memory[regs[R_PC]++];
      uint16_t op = instr >> 12;

      switch (op) {
      case OP_BR:
        BR(instr);
        break;
      case OP_ADD:
        ADD(instr);
        break;
      case OP_LD:
        LD(instr);
        break;
      case OP_ST:
        ST(instr);
        break;
      case OP_JSR:
        JSR(instr);
        break;
      case OP_AND:
        AND(instr);
        break;
      case OP_LDR:
        LDR(instr);
        break;
      case OP_STR:
        STR(instr);
        break;
      case OP_RTI:
        abort();
        break;
      case OP_NOT:
        NOT(instr);
        break;
      case OP_LDI:
        LDI(instr);
        break;
      case OP_STI:
        STI(instr);
        break;
      case OP_JMP:
        JMP(instr);
        break;
      case OP_RES:
        abort();
        break;
      case OP_LEA:
        LEA(instr);
        break;
      case OP_TRAP:
        TRAP(instr);
        break;
      }
    }
  }
};

} // namespace VM

#endif