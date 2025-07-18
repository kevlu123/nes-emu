#pragma once
#include "pch.h"
#include "bus.h"

namespace nes
{
    struct cpu_t;

    struct instruction_t
    {
        void (cpu_t::*opcode)(void);
        bool (cpu_t::*addr_mode)(void);
        int cycles;
        const char* opcode_name;
        const char* addr_mode_name;
        int byte_count() const;
    };

    struct cpu_t
    {
        cpu_t(bus_t& cpu_bus);
        void reset();
        void irq();
        void nmi();
        void clock();
        
        union
        {
            struct
            {
                bool c : 1;
                bool z : 1;
                bool i : 1;
                bool d : 1;
                bool b : 1;
                bool u : 1;
                bool v : 1;
                bool n : 1;
            };
            uint8_t reg;
        } status;
        
        uint8_t ra;
        uint8_t rx;
        uint8_t ry;
        uint8_t sp;
        uint16_t pc;

        static const instruction_t instructions[256];

        void push_stack(uint8_t value);
        uint8_t pop_stack();
        
        using addr_mode_fn = bool(void);
        using opcode_fn = void(void);
        
        addr_mode_fn IMP, IMM, ACC;
        addr_mode_fn ABS, ABX, ABY;
        addr_mode_fn ZRP, ZPX, ZPY;
        addr_mode_fn IDX, IDY, IND;
        addr_mode_fn REL;

        opcode_fn ADC, AND, ASL, BCC, BCS, BEQ, BIT, BMI, BNE, BPL, BRK, BVC, BVS, CLC;
        opcode_fn CLD, CLI, CLV, CMP, CPX, CPY, DEC, DEX, DEY, EOR, INC, INX, INY, JMP;
        opcode_fn JSR, LDA, LDX, LDY, LSR, NOP, ORA, PHA, PHP, PLA, PLP, ROL, ROR, RTI;
        opcode_fn RTS, SBC, SEC, SED, SEI, STA, STX, STY, TAX, TAY, TSX, TXA, TXS, TYA;

        opcode_fn ASL_ACC, ROL_ACC, LSR_ACC, ROR_ACC;

        opcode_fn AAC, SAX, ARR, ASR, ATX, AXA, AXS, DCP, DOP, ISB, KIL;
        opcode_fn LAR, LAX, RLA, RRA, SLO, SRE, SXA, SYA, TOP, XAA, XAS;

    private:
        bus_t* cpu_bus;
        uint16_t addr;
        bool crossed_page;
        int cycles_remaining = 0;
    };
}
