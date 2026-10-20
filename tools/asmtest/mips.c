#include "mips.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../platform/psx/gte.h"

static uint8_t ram[MIPS_RAM_SIZE];
static uint8_t scratch[1024];
uint64_t mipsInstructionCount;
uint64_t mipsMaxInstructions = 200000000ull;

#define MAX_HOOKS 32
static struct { uint32_t address; MipsHook hook; } hooks[MAX_HOOKS];
static int hookCount;

void MipsSetHook(uint32_t address, MipsHook hook)
{
    hooks[hookCount].address = address;
    hooks[hookCount].hook = hook;
    hookCount++;
}

int MipsLoadExe(const char *path)
{
    FILE *f = fopen(path, "rb");
    long size;

    if (!f) {
        return -1;
    }
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0x800, SEEK_SET);
    memset(ram, 0, sizeof(ram));
    if (fread(ram + 0x10000, 1, (size_t)(size - 0x800), f) != (size_t)(size - 0x800)) {
        fclose(f);
        return -1;
    }
    fclose(f);
    memset(scratch, 0, sizeof(scratch));
    return 0;
}

uint8_t *MipsPointer(uint32_t address)
{
    uint32_t a = address & 0x1FFFFFFFu;

    if (a < MIPS_RAM_SIZE) {
        return ram + a;
    }
    if (a >= 0x1F800000u && a < 0x1F800400u) {
        return scratch + (a - 0x1F800000u);
    }
    return NULL;
}

static uint8_t *Mem(uint32_t address, int size, uint32_t pc)
{
    uint8_t *p = MipsPointer(address);

    if (!p || (MipsPointer(address + size - 1) == NULL)) {
        fprintf(stderr, "mips: bad address %08x (size %d) at pc %08x\n", address, size, pc);
        abort();
    }
    return p;
}

uint32_t MipsLoad32(uint32_t a) { uint32_t v; memcpy(&v, Mem(a, 4, 0), 4); return v; }
uint16_t MipsLoad16(uint32_t a) { uint16_t v; memcpy(&v, Mem(a, 2, 0), 2); return v; }
uint8_t MipsLoad8(uint32_t a) { return *Mem(a, 1, 0); }
void MipsStore32(uint32_t a, uint32_t v) { memcpy(Mem(a, 4, 0), &v, 4); }
void MipsStore16(uint32_t a, uint16_t v) { memcpy(Mem(a, 2, 0), &v, 2); }
void MipsStore8(uint32_t a, uint8_t v) { *Mem(a, 1, 0) = v; }

enum { R_RA = 31, R_SP = 29, R_V0 = 2, R_A0 = 4 };

#define RETURN_ADDRESS 0xBFC00000u

static uint32_t reg[32];
static uint32_t hi, lo;

static void Unimplemented(uint32_t pc, uint32_t instr)
{
    fprintf(stderr, "mips: unimplemented instruction %08x at %08x\n", instr, pc);
    abort();
}

uint32_t MipsCall(uint32_t address, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, const uint32_t *stackArgs, int stackCount)
{
    uint32_t pc = address, nextpc;
    int pendingReg = 0;
    uint32_t pendingValue = 0;
    int i;

    memset(reg, 0, sizeof(reg));
    reg[R_SP] = 0x801FFF00u;
    for (i = 0; i < stackCount; i++) {
        MipsStore32(reg[R_SP] + 16 + 4 * (uint32_t)i, stackArgs[i]);
    }
    reg[R_A0] = a0;
    reg[R_A0 + 1] = a1;
    reg[R_A0 + 2] = a2;
    reg[R_A0 + 3] = a3;
    reg[R_RA] = RETURN_ADDRESS;
    mipsInstructionCount = 0;
    nextpc = pc + 4;

    for (;;) {
        uint32_t instr, op, rs, rt, rd, sh, fn, imm, target;
        int32_t simm;
        int branch = 0, load = 0, hasTarget = 0;
        uint32_t loadValue = 0;
        int loadReg = 0;
        uint32_t newpc;
        int h;

        if (pc == RETURN_ADDRESS) {
            return reg[R_V0];
        }
        for (h = 0; h < hookCount; h++) {
            if (hooks[h].address == pc) {
                reg[R_V0] = hooks[h].hook(reg[4], reg[5], reg[6], reg[7]);
                pc = reg[R_RA];
                nextpc = pc + 4;
                break;
            }
        }
        if (h < hookCount) {
            continue;
        }
        if (++mipsInstructionCount > mipsMaxInstructions) {
            fprintf(stderr, "mips: instruction limit at pc %08x\n", pc);
            abort();
        }
        instr = MipsLoad32(pc);
        op = instr >> 26;
        rs = (instr >> 21) & 31;
        rt = (instr >> 16) & 31;
        rd = (instr >> 11) & 31;
        sh = (instr >> 6) & 31;
        fn = instr & 63;
        imm = instr & 0xFFFF;
        simm = (int16_t)imm;
        target = (instr & 0x03FFFFFFu) << 2;
        newpc = nextpc + 4;   /* default: the instruction after the delay slot's follower */
        (void)newpc;

        /* the instruction after pc runs next; a branch changes the one after that */
        {
            uint32_t followingpc = nextpc + 4;
            uint32_t branchTarget = 0;
            int taken = 0;
            uint32_t a = reg[rs], b = reg[rt];

            switch (op) {
            case 0:
                switch (fn) {
                case 0x00: reg[rd] = b << sh; break;
                case 0x02: reg[rd] = b >> sh; break;
                case 0x03: reg[rd] = (uint32_t)((int32_t)b >> sh); break;
                case 0x04: reg[rd] = b << (a & 31); break;
                case 0x06: reg[rd] = b >> (a & 31); break;
                case 0x07: reg[rd] = (uint32_t)((int32_t)b >> (a & 31)); break;
                case 0x08: taken = 1; branchTarget = a; break;
                case 0x09: reg[rd] = pc + 8; taken = 1; branchTarget = a; break;
                case 0x10: reg[rd] = hi; break;
                case 0x11: hi = a; break;
                case 0x12: reg[rd] = lo; break;
                case 0x13: lo = a; break;
                case 0x18: { int64_t p = (int64_t)(int32_t)a * (int32_t)b; lo = (uint32_t)p; hi = (uint32_t)((uint64_t)p >> 32); break; }
                case 0x19: { uint64_t p = (uint64_t)a * b; lo = (uint32_t)p; hi = (uint32_t)(p >> 32); break; }
                case 0x1A:
                    if (b == 0) { lo = (int32_t)a < 0 ? 1 : 0xFFFFFFFFu; hi = a; }
                    else if (a == 0x80000000u && b == 0xFFFFFFFFu) { lo = 0x80000000u; hi = 0; }
                    else { lo = (uint32_t)((int32_t)a / (int32_t)b); hi = (uint32_t)((int32_t)a % (int32_t)b); }
                    break;
                case 0x1B:
                    if (b == 0) { lo = 0xFFFFFFFFu; hi = a; }
                    else { lo = a / b; hi = a % b; }
                    break;
                case 0x20: case 0x21: reg[rd] = a + b; break;
                case 0x22: case 0x23: reg[rd] = a - b; break;
                case 0x24: reg[rd] = a & b; break;
                case 0x25: reg[rd] = a | b; break;
                case 0x26: reg[rd] = a ^ b; break;
                case 0x27: reg[rd] = ~(a | b); break;
                case 0x2A: reg[rd] = (int32_t)a < (int32_t)b; break;
                case 0x2B: reg[rd] = a < b; break;
                default: Unimplemented(pc, instr);
                }
                break;
            case 1: {
                int link = (rt & 0x10) != 0;
                int cond = (rt & 1) ? ((int32_t)a >= 0) : ((int32_t)a < 0);

                if (link) {
                    reg[R_RA] = pc + 8;
                }
                if (cond) { taken = 1; branchTarget = pc + 4 + ((uint32_t)simm << 2); }
                break;
            }
            case 2: taken = 1; branchTarget = (pc & 0xF0000000u) | target; break;
            case 3: reg[R_RA] = pc + 8; taken = 1; branchTarget = (pc & 0xF0000000u) | target; break;
            case 4: if (a == b) { taken = 1; branchTarget = pc + 4 + ((uint32_t)simm << 2); } break;
            case 5: if (a != b) { taken = 1; branchTarget = pc + 4 + ((uint32_t)simm << 2); } break;
            case 6: if ((int32_t)a <= 0) { taken = 1; branchTarget = pc + 4 + ((uint32_t)simm << 2); } break;
            case 7: if ((int32_t)a > 0) { taken = 1; branchTarget = pc + 4 + ((uint32_t)simm << 2); } break;
            case 8: case 9: reg[rt] = a + (uint32_t)simm; break;
            case 10: reg[rt] = (int32_t)a < simm; break;
            case 11: reg[rt] = a < (uint32_t)simm; break;
            case 12: reg[rt] = a & imm; break;
            case 13: reg[rt] = a | imm; break;
            case 14: reg[rt] = a ^ imm; break;
            case 15: reg[rt] = imm << 16; break;
            case 0x12: /* COP2 */
                if (instr & 0x02000000u) {
                    GteExecute(instr);
                } else {
                    switch (rs) {
                    case 0: load = 1; loadReg = (int)rt; loadValue = (uint32_t)GteReadData((int)rd); break;
                    case 2: load = 1; loadReg = (int)rt; loadValue = (uint32_t)GteReadControl((int)rd); break;
                    case 4: GteWriteData((int)rd, (int32_t)reg[rt]); break;
                    case 6: GteWriteControl((int)rd, (int32_t)reg[rt]); break;
                    default: Unimplemented(pc, instr);
                    }
                }
                break;
            case 0x20: load = 1; loadReg = (int)rt; loadValue = (uint32_t)(int32_t)(int8_t)MipsLoad8(a + (uint32_t)simm); break;
            case 0x21: load = 1; loadReg = (int)rt; loadValue = (uint32_t)(int32_t)(int16_t)MipsLoad16(a + (uint32_t)simm); break;
            case 0x23: load = 1; loadReg = (int)rt; loadValue = MipsLoad32(a + (uint32_t)simm); break;
            case 0x24: load = 1; loadReg = (int)rt; loadValue = MipsLoad8(a + (uint32_t)simm); break;
            case 0x25: load = 1; loadReg = (int)rt; loadValue = MipsLoad16(a + (uint32_t)simm); break;
            case 0x22: { /* lwl: the register's low bytes are kept, the high bytes come from the word's low bytes */
                uint32_t ad = a + (uint32_t)simm, word = MipsLoad32(ad & ~3u), cur = (pendingReg == (int)rt) ? pendingValue : reg[rt];
                switch (ad & 3) {
                case 0: loadValue = (cur & 0x00FFFFFFu) | (word << 24); break;
                case 1: loadValue = (cur & 0x0000FFFFu) | (word << 16); break;
                case 2: loadValue = (cur & 0x000000FFu) | (word << 8); break;
                default: loadValue = word; break;
                }
                load = 1; loadReg = (int)rt;
                break;
            }
            case 0x26: { /* lwr */
                uint32_t ad = a + (uint32_t)simm, word = MipsLoad32(ad & ~3u), cur = (pendingReg == (int)rt) ? pendingValue : reg[rt];
                switch (ad & 3) {
                case 0: loadValue = word; break;
                case 1: loadValue = (cur & 0xFF000000u) | (word >> 8); break;
                case 2: loadValue = (cur & 0xFFFF0000u) | (word >> 16); break;
                default: loadValue = (cur & 0xFFFFFF00u) | (word >> 24); break;
                }
                load = 1; loadReg = (int)rt;
                break;
            }
            case 0x28: MipsStore8(a + (uint32_t)simm, (uint8_t)b); break;
            case 0x29: MipsStore16(a + (uint32_t)simm, (uint16_t)b); break;
            case 0x2B: MipsStore32(a + (uint32_t)simm, b); break;
            case 0x2A: { /* swl */
                uint32_t ad = a + (uint32_t)simm, word = MipsLoad32(ad & ~3u);
                switch (ad & 3) {
                case 0: word = (word & 0xFFFFFF00u) | (b >> 24); break;
                case 1: word = (word & 0xFFFF0000u) | (b >> 16); break;
                case 2: word = (word & 0xFF000000u) | (b >> 8); break;
                default: word = b; break;
                }
                MipsStore32(ad & ~3u, word);
                break;
            }
            case 0x2E: { /* swr */
                uint32_t ad = a + (uint32_t)simm, word = MipsLoad32(ad & ~3u);
                switch (ad & 3) {
                case 0: word = b; break;
                case 1: word = (word & 0x000000FFu) | (b << 8); break;
                case 2: word = (word & 0x0000FFFFu) | (b << 16); break;
                default: word = (word & 0x00FFFFFFu) | (b << 24); break;
                }
                MipsStore32(ad & ~3u, word);
                break;
            }
            case 0x32: load = 0; GteWriteData((int)rt, (int32_t)MipsLoad32(a + (uint32_t)simm)); break; /* lwc2 */
            case 0x3A: MipsStore32(a + (uint32_t)simm, (uint32_t)GteReadData((int)rt)); break;          /* swc2 */
            default: Unimplemented(pc, instr);
            }
            (void)followingpc;
            branch = taken;
            hasTarget = taken;
            /* A load's result is visible to the instruction after the next one: commit the previous load now
               (unless this instruction wrote the same register), then remember this one. */
            if (pendingReg) {
                reg[pendingReg] = pendingValue;
            }
            pendingReg = 0;
            if (load && loadReg) {
                pendingReg = loadReg;
                pendingValue = loadValue;
            }
            reg[0] = 0;
            pc = nextpc;
            nextpc = hasTarget ? branchTarget : pc + 4;
            (void)branch;
        }
    }
}
