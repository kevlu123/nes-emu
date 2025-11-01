#include "pch.h"
#include "cpu.h"

static uint8_t page_differs(uint16_t addr1, uint16_t addr2)
{
    return (addr1 & 0xFF00) != (addr2 & 0xFF00);
}

namespace nes
{
#define X(_opcode, _addr_mode, _cycles)                                          \
    instruction_t{                                                               \
        .opcode = &cpu_t::_opcode,                                               \
        .addr_mode = &cpu_t::_addr_mode,                                         \
        .cycles = _cycles,                                                       \
        .opcode_name = { #_opcode[0], #_opcode[1], #_opcode[2], 0 },             \
        .addr_mode_name = { #_addr_mode[0], #_addr_mode[1], #_addr_mode[2], 0 }, \
    }

    const instruction_t cpu_t::instructions[256]
    {
    /* YX*/ /* 0 */         /* 1 */         /* 2 */         /* 3 */         /* 4 */         /* 5 */         /* 6 */         /* 7 */         /* 8 */         /* 9 */         /* A */             /* B */         /* C */         /* D */         /* E */         /* F */
    /* 0 */ X(BRK, IMM, 7), X(ORA, IDX, 6), X(KIL, IMP, 2), X(SLO, IDX, 8), X(DOP, ZRP, 3), X(ORA, ZRP, 3), X(ASL, ZRP, 5), X(SLO, ZRP, 5), X(PHP, IMP, 3), X(ORA, IMM, 2), X(ASL_ACC, ACC, 2), X(AAC, IMM, 2), X(TOP, ABS, 4), X(ORA, ABS, 4), X(ASL, ABS, 6), X(SLO, ABS, 6),
    /* 1 */ X(BPL, REL, 2), X(ORA, IDY, 5), X(KIL, IMP, 2), X(SLO, IDY, 8), X(DOP, ZPX, 4), X(ORA, ZPX, 4), X(ASL, ZPX, 6), X(SLO, ZPX, 6), X(CLC, IMP, 2), X(ORA, ABY, 4), X(NOP,     IMP, 2), X(SLO, ABY, 7), X(TOP, ABX, 4), X(ORA, ABX, 4), X(ASL, ABX, 7), X(SLO, ABX, 7),
    /* 2 */ X(JSR, ABS, 6), X(AND, IDX, 6), X(KIL, IMP, 2), X(RLA, IDX, 8), X(BIT, ZRP, 3), X(AND, ZRP, 3), X(ROL, ZRP, 5), X(RLA, ZRP, 5), X(PLP, IMP, 4), X(AND, IMM, 2), X(ROL_ACC, ACC, 2), X(AAC, IMM, 2), X(BIT, ABS, 4), X(AND, ABS, 4), X(ROL, ABS, 6), X(RLA, ABS, 6),
    /* 3 */ X(BMI, REL, 2), X(AND, IDY, 5), X(KIL, IMP, 2), X(RLA, IDY, 8), X(DOP, ZPX, 4), X(AND, ZPX, 4), X(ROL, ZPX, 6), X(RLA, ZPX, 6), X(SEC, IMP, 2), X(AND, ABY, 4), X(NOP,     IMP, 2), X(RLA, ABY, 7), X(TOP, ABX, 4), X(AND, ABX, 4), X(ROL, ABX, 7), X(RLA, ABX, 7),

    /* 4 */ X(RTI, IMP, 6), X(EOR, IDX, 6), X(KIL, IMP, 2), X(SRE, IDX, 8), X(DOP, ZRP, 3), X(EOR, ZRP, 3), X(LSR, ZRP, 5), X(SRE, ZRP, 5), X(PHA, IMP, 3), X(EOR, IMM, 2), X(LSR_ACC, ACC, 2), X(ASR, IMM, 2), X(JMP, ABS, 3), X(EOR, ABS, 4), X(LSR, ABS, 6), X(SRE, ABS, 6),
    /* 5 */ X(BVC, REL, 2), X(EOR, IDY, 5), X(KIL, IMP, 2), X(SRE, IDY, 8), X(DOP, ZPX, 4), X(EOR, ZPX, 4), X(LSR, ZPX, 6), X(SRE, ZPX, 6), X(CLI, IMP, 2), X(EOR, ABY, 4), X(NOP,     IMP, 2), X(SRE, ABY, 7), X(TOP, ABX, 4), X(EOR, ABX, 4), X(LSR, ABX, 7), X(SRE, ABX, 7),
    /* 6 */ X(RTS, IMP, 6), X(ADC, IDX, 6), X(KIL, IMP, 2), X(RRA, IDX, 8), X(DOP, ZRP, 3), X(ADC, ZRP, 3), X(ROR, ZRP, 5), X(RRA, ZRP, 5), X(PLA, IMP, 4), X(ADC, IMM, 2), X(ROR_ACC, ACC, 2), X(ARR, IMM, 2), X(JMP, IND, 5), X(ADC, ABS, 4), X(ROR, ABS, 6), X(RRA, ABS, 6),
    /* 7 */ X(BVS, REL, 2), X(ADC, IDY, 5), X(KIL, IMP, 2), X(RRA, IDY, 8), X(DOP, ZPX, 4), X(ADC, ZPX, 4), X(ROR, ZPX, 6), X(RRA, ZPX, 6), X(SEI, IMP, 2), X(ADC, ABY, 4), X(NOP,     IMP, 2), X(RRA, ABY, 7), X(TOP, ABX, 4), X(ADC, ABX, 4), X(ROR, ABX, 7), X(RRA, ABX, 7),

    /* 8 */ X(DOP, IMM, 2), X(STA, IDX, 6), X(DOP, IMM, 2), X(SAX, IDX, 6), X(STY, ZRP, 3), X(STA, ZRP, 3), X(STX, ZRP, 3), X(SAX, ZRP, 3), X(DEY, IMP, 2), X(DOP, IMM, 2), X(TXA,     IMP, 2), X(XAA, IMM, 2), X(STY, ABS, 4), X(STA, ABS, 4), X(STX, ABS, 4), X(SAX, ABS, 4),
    /* 9 */ X(BCC, REL, 2), X(STA, IDY, 6), X(KIL, IMP, 2), X(AXA, IDY, 6), X(STY, ZPX, 4), X(STA, ZPX, 4), X(STX, ZPY, 4), X(SAX, ZPY, 4), X(TYA, IMP, 2), X(STA, ABY, 5), X(TXS,     IMP, 2), X(XAS, ABY, 5), X(SYA, ABX, 5), X(STA, ABX, 5), X(SXA, ABY, 5), X(AXA, ABY, 5),
    /* A */ X(LDY, IMM, 2), X(LDA, IDX, 6), X(LDX, IMM, 2), X(LAX, IDX, 6), X(LDY, ZRP, 3), X(LDA, ZRP, 3), X(LDX, ZRP, 3), X(LAX, ZRP, 3), X(TAY, IMP, 2), X(LDA, IMM, 2), X(TAX,     IMP, 2), X(ATX, IMM, 2), X(LDY, ABS, 4), X(LDA, ABS, 4), X(LDX, ABS, 4), X(LAX, ABS, 4),
    /* B */ X(BCS, REL, 2), X(LDA, IDY, 5), X(KIL, IMP, 2), X(LAX, IDY, 5), X(LDY, ZPX, 4), X(LDA, ZPX, 4), X(LDX, ZPY, 4), X(LAX, ZPY, 4), X(CLV, IMP, 2), X(LDA, ABY, 4), X(TSX,     IMP, 2), X(LAR, ABY, 4), X(LDY, ABX, 4), X(LDA, ABX, 4), X(LDX, ABY, 4), X(LAX, ABY, 4),

    /* C */ X(CPY, IMM, 2), X(CMP, IDX, 6), X(DOP, IMM, 2), X(DCP, IDX, 8), X(CPY, ZRP, 3), X(CMP, ZRP, 3), X(DEC, ZRP, 5), X(DCP, ZRP, 5), X(INY, IMP, 2), X(CMP, IMM, 2), X(DEX,     IMP, 2), X(AXS, IMM, 2), X(CPY, ABS, 4), X(CMP, ABS, 4), X(DEC, ABS, 6), X(DCP, ABS, 6),
    /* D */ X(BNE, REL, 2), X(CMP, IDY, 5), X(KIL, IMP, 2), X(DCP, IDY, 8), X(DOP, ZPX, 4), X(CMP, ZPX, 4), X(DEC, ZPX, 6), X(DCP, ZPX, 6), X(CLD, IMP, 2), X(CMP, ABY, 4), X(NOP,     IMP, 2), X(DCP, ABY, 7), X(TOP, ABX, 4), X(CMP, ABX, 4), X(DEC, ABX, 7), X(DCP, ABX, 7),
    /* E */ X(CPX, IMM, 2), X(SBC, IDX, 6), X(DOP, IMM, 2), X(ISB, IDX, 8), X(CPX, ZRP, 3), X(SBC, ZRP, 3), X(INC, ZRP, 5), X(ISB, ZRP, 5), X(INX, IMP, 2), X(SBC, IMM, 2), X(NOP,     IMP, 2), X(SBC, IMM, 2), X(CPX, ABS, 4), X(SBC, ABS, 4), X(INC, ABS, 6), X(ISB, ABS, 6),
    /* F */ X(BEQ, REL, 2), X(SBC, IDY, 5), X(KIL, IMP, 2), X(ISB, IDY, 8), X(DOP, ZPX, 4), X(SBC, ZPX, 4), X(INC, ZPX, 6), X(ISB, ZPX, 6), X(SED, IMP, 2), X(SBC, ABY, 4), X(NOP,     IMP, 2), X(ISB, ABY, 7), X(TOP, ABX, 4), X(SBC, ABX, 4), X(INC, ABX, 7), X(ISB, ABX, 7),
    };
#undef X

    int instruction_t::byte_count() const
    {
        if (addr_mode == &cpu_t::IMP || addr_mode == &cpu_t::ACC)
        {
            return 1;
        }
        else if (addr_mode == &cpu_t::ABS
              || addr_mode == &cpu_t::ABX
              || addr_mode == &cpu_t::ABY
              || addr_mode == &cpu_t::IND)
        {
            return 3;
        }
        else
        {
            return 2;
        }
    }

    cpu_t::cpu_t(bus_t &cpu_bus)
        : cpu_bus(&cpu_bus),
          status{},
          ra(0),
          rx(0),
          ry(0),
          sp(0xFD),
          pc(0),
          addr(0),
          crossed_page(false),
          cycles_until_next_instruction(0)
    {
        status.i = true;
        status.u = true;
        pc = cpu_bus.read(0xFFFC) | (cpu_bus.read(0xFFFD) << 8);
    }

    void cpu_t::reset()
    {
        *this = cpu_t(*cpu_bus);
    }

    void cpu_t::irq()
    {
        if (!status.i)
        {
            push_stack((uint8_t)(pc >> 8));
            push_stack((uint8_t)pc);
            status.b = false;
            push_stack(status.reg);
            status.i = true;
            pc = cpu_bus->read(0xFFFE) | (cpu_bus->read(0xFFFF) << 8);
            cycles_until_next_instruction = 7;
        }
    }

    void cpu_t::nmi()
    {
        push_stack((uint8_t)(pc >> 8));
        push_stack((uint8_t)pc);
        status.b = false;
        push_stack(status.reg);
        status.i = true;
        pc = cpu_bus->read(0xFFFA) | (cpu_bus->read(0xFFFB) << 8);
        cycles_until_next_instruction = 8;
    }

    void cpu_t::clock()
    {
        if (cycles_until_next_instruction == 0)
        {
            uint8_t op = cpu_bus->read(pc);
            pc++;
            const instruction_t &instruction = instructions[op];
            cycles_until_next_instruction = instruction.cycles;
            crossed_page = (this->*instruction.addr_mode)();
            (this->*instruction.opcode)();
        }
        cycles_until_next_instruction--;
    }

    void cpu_t::push_stack(uint8_t value)
    {
        cpu_bus->write(0x0100 + sp, value);
        sp--;
    }

    uint8_t cpu_t::pop_stack()
    {
        sp++;
        return cpu_bus->read(0x0100 + sp);
    }

    // Addressing modes

    // Implicit
    bool cpu_t::IMP()
    {
        return false;
    }

    // Immediate
    // addr = pc
    bool cpu_t::IMM()
    {
        addr = pc;
        pc++;
        return false;
    }

    // Absolute
    // addr = [pc]
    bool cpu_t::ABS()
    {
        addr = cpu_bus->read(pc) | (cpu_bus->read(pc + 1) << 8);
        pc += 2;
        return false;
    }

    // X-indexed absolute
    // addr = [pc] + rx
    bool cpu_t::ABX()
    {
        uint16_t tmp_addr = cpu_bus->read(pc) | (cpu_bus->read(pc + 1) << 8);
        addr = tmp_addr + rx;
        pc += 2;
        return page_differs(tmp_addr, addr);
    }

    // Y-indexed absolute
    // addr = [pc] + ry
    bool cpu_t::ABY()
    {
        uint16_t tmp_addr = cpu_bus->read(pc) | (cpu_bus->read(pc + 1) << 8);
        addr = tmp_addr + ry;
        pc += 2;
        return page_differs(tmp_addr, addr);
    }

    // Zero page
    // addr = [pc] & 0xFF
    bool cpu_t::ZRP()
    {
        addr = cpu_bus->read(pc);
        pc++;
        return false;
    }

    // X-indexed zero page
    // addr = ([pc] + rx) & 0xFF
    bool cpu_t::ZPX()
    {
        addr = (cpu_bus->read(pc) + rx) & 0xFF;
        pc++;
        return false;
    }

    // Y-indexed zero page
    // addr = ([pc] + ry) & 0xFF
    bool cpu_t::ZPY()
    {
        addr = (cpu_bus->read(pc) + ry) & 0xFF;
        pc++;
        return false;
    }

    // Accumulator
    bool cpu_t::ACC()
    {
        return false;
    }

    // Relative (for branches)
    // addr = (signed [pc] & 0xFF) + pc + 1
    bool cpu_t::REL()
    {
        addr = (int8_t)cpu_bus->read(pc) + pc + 1;
        pc++;
        return page_differs(pc + 1, addr);
    }

    // Indexed indirect
    // data = [([pc] + rx) & 0xFF]
    bool cpu_t::IDX()
    {
        uint8_t zrp_addr = cpu_bus->read(pc) + rx;
        pc++;
        addr = cpu_bus->read(zrp_addr) |
               (cpu_bus->read((zrp_addr + 1) & 0xFF) << 8);
        return false;
    }

    // Indirect indexed
    // data = [[pc] & 0xFF] + ry
    bool cpu_t::IDY()
    {
        uint8_t zrp_addr = cpu_bus->read(pc);
        pc++;
        uint16_t tmp_addr = cpu_bus->read(zrp_addr) |
                      (cpu_bus->read((zrp_addr + 1) & 0xFF) << 8);
        addr = tmp_addr + ry;
        return page_differs(tmp_addr, addr);
    }

    // Indirect (for JMP)
    // addr = [[pc]]
    bool cpu_t::IND()
    {
        addr = cpu_bus->read(pc) | (cpu_bus->read(pc + 1) << 8);
        addr = cpu_bus->read(addr) |
               (cpu_bus->read(((addr + 1) & 0xFF) | (addr & 0xFF00)) << 8);
        return false;
    }

    // Opcodes

    // Add with carry
    void cpu_t::ADC()
    {
        uint8_t data = cpu_bus->read(addr);
        uint16_t res16 = ra + data + status.c;
        uint8_t res = (uint8_t)res16;
        status.c = res16 > 255;
        status.z = res == 0;
        status.n = res & 0x80;
        status.v = (~(ra ^ data) & (ra ^ res)) & 0x80;
        ra = res;
        cycles_until_next_instruction += crossed_page;
    }

    // Bitwise AND
    void cpu_t::AND()
    {
        uint8_t data = cpu_bus->read(addr);
        ra &= data;
        status.n = ra & 0x80;
        status.z = ra == 0;
        cycles_until_next_instruction += crossed_page;
    }

    // Arithmetic shift left (memory)
    void cpu_t::ASL()
    {
        uint8_t data = cpu_bus->read(addr);
        uint8_t res = data << 1;
        cpu_bus->write(addr, res);
        status.c = data & 0x80;
        status.n = res & 0x80;
        status.z = res == 0;
    }

    // Arithmetic shift left (accumulator)
    void cpu_t::ASL_ACC()
    {
        bool carry = ra & 0x80;
        ra <<= 1;
        status.c = carry;
        status.n = ra & 0x80;
        status.z = ra == 0;
    }

    // Branch if carry clear
    void cpu_t::BCC()
    {
        if (!status.c)
        {
            pc = addr;
            cycles_until_next_instruction += crossed_page + 1;
        }
    }

    // Branch if carry set
    void cpu_t::BCS()
    {
        if (status.c)
        {
            pc = addr;
            cycles_until_next_instruction += crossed_page + 1;
        }
    }

    // Branch if equal
    void cpu_t::BEQ()
    {
        if (status.z)
        {
            pc = addr;
            cycles_until_next_instruction += crossed_page + 1;
        }
    }

    // Bit test
    void cpu_t::BIT()
    {
        uint8_t data = cpu_bus->read(addr);
        status.z = (data & ra) == 0;
        status.n = data & 0x80;
        status.v = data & 0x40;
    }

    // Branch if minus
    void cpu_t::BMI()
    {
        if (status.n)
        {
            pc = addr;
            cycles_until_next_instruction += crossed_page + 1;
        }
    }

    // Branch if not equal
    void cpu_t::BNE()
    {
        if (!status.z)
        {
            pc = addr;
            cycles_until_next_instruction += crossed_page + 1;
        }
    }

    // Branch if plus
    void cpu_t::BPL()
    {
        if (!status.n)
        {
            pc = addr;
            cycles_until_next_instruction += crossed_page + 1;
        }
    }

    // Break (software IRQ)
    void cpu_t::BRK()
    {
        push_stack((uint8_t)(pc >> 8));
        push_stack((uint8_t)pc);
        status.b = true;
        push_stack(status.reg);
        status.i = true;
        pc = cpu_bus->read(0xFFFE) | (cpu_bus->read(0xFFFF) << 8);
    }

    // Branch if overflow clear
    void cpu_t::BVC()
    {
        if (!status.v)
        {
            pc = addr;
            cycles_until_next_instruction += crossed_page + 1;
        }
    }

    // Branch if overflow set
    void cpu_t::BVS()
    {
        if (status.v)
        {
            pc = addr;
            cycles_until_next_instruction += crossed_page + 1;
        }
    }

    // Clear carry
    void cpu_t::CLC()
    {
        status.c = false;
    }

    // Clear decimal
    void cpu_t::CLD()
    {
        status.d = false;
    }

    // Clear interrupt disable
    void cpu_t::CLI()
    {
        status.i = false;
    }

    // Clear overflow
    void cpu_t::CLV()
    {
        status.v = false;
    }

    // Compare A
    void cpu_t::CMP()
    {
        uint8_t data = cpu_bus->read(addr);
        uint8_t res = ra - data;
        status.n = res & 0x80;
        status.z = res == 0;
        status.c = ra >= data;
        cycles_until_next_instruction += crossed_page;
    }

    // Compare X
    void cpu_t::CPX()
    {
        uint8_t data = cpu_bus->read(addr);
        uint8_t res = rx - data;
        status.n = res & 0x80;
        status.z = res == 0;
        status.c = rx >= data;
    }

    // Compare Y
    void cpu_t::CPY()
    {
        uint8_t data = cpu_bus->read(addr);
        uint8_t res = ry - data;
        status.n = res & 0x80;
        status.z = res == 0;
        status.c = ry >= data;
    }

    // Decrement memory
    void cpu_t::DEC()
    {
        uint8_t data = cpu_bus->read(addr);
        uint8_t res = data - 1;
        cpu_bus->write(addr, res);
        status.n = res & 0x80;
        status.z = res == 0;
    }

    // Decrement X
    void cpu_t::DEX()
    {
        rx--;
        status.n = rx & 0x80;
        status.z = rx == 0;
    }

    // Decrement Y
    void cpu_t::DEY()
    {
        ry--;
        status.n = ry & 0x80;
        status.z = ry == 0;
    }

    // Bitwise exclusive OR
    void cpu_t::EOR()
    {
        uint8_t data = cpu_bus->read(addr);
        ra ^= data;
        status.n = ra & 0x80;
        status.z = ra == 0;
        cycles_until_next_instruction += crossed_page;
    }

    // Increment memory
    void cpu_t::INC()
    {
        uint8_t data = cpu_bus->read(addr);
        uint8_t res = data + 1;
        cpu_bus->write(addr, res);
        status.n = res & 0x80;
        status.z = res == 0;
    }

    // Increment X
    void cpu_t::INX()
    {
        rx++;
        status.n = rx & 0x80;
        status.z = rx == 0;
    }

    // Increment Y
    void cpu_t::INY()
    {
        ry++;
        status.n = ry & 0x80;
        status.z = ry == 0;
    }

    // Jump
    void cpu_t::JMP()
    {
        pc = addr;
    }

    // Jump to subroutine
    void cpu_t::JSR()
    {
        uint16_t prev_addr = pc - 1;
        push_stack((uint8_t)(prev_addr >> 8));
        push_stack((uint8_t)prev_addr);
        pc = addr;
    }

    // Load A
    void cpu_t::LDA()
    {
        ra = cpu_bus->read(addr);
        status.z = ra == 0;
        status.n = ra & 0x80;
        cycles_until_next_instruction += crossed_page;
    }

    // Load X
    void cpu_t::LDX()
    {
        rx = cpu_bus->read(addr);
        status.z = rx == 0;
        status.n = rx & 0x80;
        cycles_until_next_instruction += crossed_page;
    }

    // Load Y
    void cpu_t::LDY()
    {
        ry = cpu_bus->read(addr);
        status.z = ry == 0;
        status.n = ry & 0x80;
        cycles_until_next_instruction += crossed_page;
    }

    // Logical shift left (memory)
    void cpu_t::LSR()
    {
        uint8_t data = cpu_bus->read(addr);
        uint8_t res = data >> 1;
        cpu_bus->write(addr, res);
        status.c = data & 1;
        status.n = res & 0x80;
        status.z = res == 0;
    }

    // Logical shift right (accumulator)
    void cpu_t::LSR_ACC()
    {
        bool carry = ra & 0x01;
        ra >>= 1;
        status.c = carry;
        status.n = ra & 0x80;
        status.z = ra == 0;
    }

    // No operation
    void cpu_t::NOP()
    {
    }

    // Bitwise OR
    void cpu_t::ORA()
    {
        uint8_t data = cpu_bus->read(addr);
        ra |= data;
        status.n = ra & 0x80;
        status.z = ra == 0;
        cycles_until_next_instruction += crossed_page;
    }

    // Push A
    void cpu_t::PHA()
    {
        push_stack(ra);
    }

    // Push processor status
    void cpu_t::PHP()
    {
        status.b = true;
        push_stack(status.reg);
    }

    // Pull A
    void cpu_t::PLA()
    {
        ra = pop_stack();
        status.z = ra == 0;
        status.n = ra & 0x80;
    }

    // Pull processor status
    void cpu_t::PLP()
    {
        status.reg = pop_stack();
        status.u = true;
    }

    // Rotate left (memory)
    void cpu_t::ROL()
    {
        uint8_t data = cpu_bus->read(addr);
        uint8_t res = (data << 1) | (uint8_t)status.c;
        cpu_bus->write(addr, res);
        status.c = data & 0x80;
        status.n = res & 0x80;
        status.z = res == 0;
    }

    // Rotate left (accumulator)
    void cpu_t::ROL_ACC()
    {
        bool carry = ra & 0x80;
        ra = (ra << 1) | (uint8_t)status.c;
        status.c = carry;
        status.n = ra & 0x80;
        status.z = ra == 0;
    }

    // Rotate right (memory)
    void cpu_t::ROR()
    {
        uint8_t data = cpu_bus->read(addr);
        uint8_t res = (data >> 1) | (status.c << 7);
        cpu_bus->write(addr, res);
        status.c = data & 0x01;
        status.n = res & 0x80;
        status.z = res == 0;
    }

    // Rotate right (accumulator)
    void cpu_t::ROR_ACC()
    {
        bool carry = ra & 0x01;
        ra = (ra >> 1) | (status.c << 7);
        status.c = carry;
        status.n = ra & 0x80;
        status.z = ra == 0;
    }

    // Return from interrupt
    void cpu_t::RTI()
    {
        status.reg = pop_stack();
        status.u = true;
        pc = pop_stack();
        pc |= pop_stack() << 8;
    }

    // Return from subroutine
    void cpu_t::RTS()
    {
        pc = pop_stack();
        pc |= pop_stack() << 8;
        pc++;
    }

    // Subtract with carry
    void cpu_t::SBC()
    {
        uint8_t data = cpu_bus->read(addr);
        data = ~data;
        uint16_t res16 = ra + data + status.c;
        uint8_t res = (uint8_t)res16;
        status.c = res16 > 255;
        status.z = res == 0;
        status.n = res & 0x80;
        status.v = (~(ra ^ data) & (ra ^ res)) & 0x80;
        ra = res;
        cycles_until_next_instruction += crossed_page;
    }

    // Set carry
    void cpu_t::SEC()
    {
        status.c = true;
    }

    // Set decimal
    void cpu_t::SED()
    {
        status.d = true;
    }

    // Set interrupt disable
    void cpu_t::SEI()
    {
        status.i = true;
    }

    // Store A
    void cpu_t::STA()
    {
        cpu_bus->write(addr, ra);
    }

    // Store X
    void cpu_t::STX()
    {
        cpu_bus->write(addr, rx);
    }

    // Store Y
    void cpu_t::STY()
    {
        cpu_bus->write(addr, ry);
    }

    // Transfer A to X
    void cpu_t::TAX()
    {
        rx = ra;
        status.n = rx & 0x80;
        status.z = rx == 0;
    }

    // Transfer A to Y
    void cpu_t::TAY()
    {
        ry = ra;
        status.n = ry & 0x80;
        status.z = ry == 0;
    }

    // Transfer stack pointer to X
    void cpu_t::TSX()
    {
        rx = sp;
        status.n = rx & 0x80;
        status.z = rx == 0;
    }

    // Transfer X to A
    void cpu_t::TXA()
    {
        ra = rx;
        status.n = ra & 0x80;
        status.z = ra == 0;
    }

    // Transfer X to stack pointer
    void cpu_t::TXS()
    {
        sp = rx;
    }

    // Transfer Y to A
    void cpu_t::TYA()
    {
        ra = ry;
        status.n = ra & 0x80;
        status.z = ra == 0;
    }

    // Illegal opcodes

    void cpu_t::AAC()
    {
        AND();
        status.c = status.z;
    }

    void cpu_t::SAX()
    {
        cpu_bus->write(addr, rx & ra);
    }

    void cpu_t::ARR()
    {
        uint8_t data = cpu_bus->read(addr);
        ra &= data;
        ra = (ra >> 1) | (ra << 7);
        status.n = ra & 0x80;
        status.z = ra == 0;
        bool b5 = ra & 0b0010'0000;
        bool b6 = ra & 0b0100'0000;
        status.c = b6;
        status.v = b5 ^ b6;
    }

    void cpu_t::ASR()
    {
        uint8_t data = cpu_bus->read(addr);
        ra &= data;
        status.c = ra & 1;
        ra >>= 1;
        status.n = ra & 0x80;
        status.z = ra == 0;
    }

    void cpu_t::ATX()
    {
        uint8_t data = cpu_bus->read(addr);
        ra &= data;
        rx = ra;
        status.n = rx & 0x80;
        status.z = rx == 0;
    }

    void cpu_t::AXA()
    {
        cpu_bus->write(addr, ra & rx & 7);
    }

    void cpu_t::AXS()
    {
        uint8_t data = cpu_bus->read(addr);
        rx &= ra;
        status.c = rx < data;
        rx -= data;
        status.n = rx & 0x80;
        status.z = rx == 0;
    }

    void cpu_t::DCP()
    {
        DEC();
        CMP();
    }

    void cpu_t::DOP()
    {
    }

    void cpu_t::ISB()
    {
        uint8_t data = cpu_bus->read(addr) + 1;
        cpu_bus->write(addr, data);
        SBC();
    }

    void cpu_t::KIL()
    {
        pc--;
    }

    void cpu_t::LAR()
    {
        uint8_t data = cpu_bus->read(addr);
        ra = rx = sp &= data;
        status.n = ra & 0x80;
        status.z = ra == 0;
    }

    void cpu_t::LAX()
    {
        ra = rx = cpu_bus->read(addr);
        status.n = ra & 0x80;
        status.z = ra == 0;
    }

    void cpu_t::RLA()
    {
        ROL();
        AND();
    }

    void cpu_t::RRA()
    {
        ROR();
        ADC();
    }

    void cpu_t::SLO()
    {
        ASL();
        ORA();
    }

    void cpu_t::SRE()
    {
        LSR();
        EOR();
    }

    void cpu_t::SXA()
    {
        cpu_bus->write(addr, ((addr >> 8) & rx) + 1);
    }

    void cpu_t::SYA()
    {
        cpu_bus->write(addr, ((addr >> 8) & ry) + 1);
    }

    void cpu_t::TOP()
    {
    }

    void cpu_t::XAA()
    {
        // Unpredictable
        uint8_t data = cpu_bus->read(addr);
        ra &= rx & data;
    }

    void cpu_t::XAS()
    {
        sp = rx & ra;
        cpu_bus->write(addr, ((addr >> 8) & rx) + 1);
    }
}
