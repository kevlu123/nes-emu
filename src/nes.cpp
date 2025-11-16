
#include "pch.h"
#include "nes.h"

namespace nes
{
    nes_t::nes_t(std::function<void()> on_clock)
        : cpu(cpu_bus),
          ppu(ppu_bus, oam_dma, screen_buffer),
          apu(cpu_bus),
          oam_dma(cpu_bus),
          ppu_clock_count(0),
          on_clock(on_clock),
          cpu_bus(0xFFFF, "CPU"),
          ppu_bus(0x3FFF, "PPU")
    {
        cpu_bus.connect_read<&ram_t::read>(&ram);
        cpu_bus.connect_write<&ram_t::write>(&ram);
        cpu_bus.connect_read<&ppu_t::cpu_read>(&ppu);
        cpu_bus.connect_write<&ppu_t::cpu_write>(&ppu);
        cpu_bus.connect_read<&apu_t::read>(&apu);
        cpu_bus.connect_write<&apu_t::write>(&apu);
        cpu_bus.connect_read<&controller_t::read>(&controller);
        cpu_bus.connect_write<&controller_t::write>(&controller);
    }

    void nes_t::reset()
    {
        ram.reset();
        cpu.reset();
        ppu.reset();
        apu.reset();
        oam_dma.reset();
        controller.reset();
        if (cart)
        {
            cart->reset();
        }
        ppu_clock_count = 0;
        memset(screen_buffer, 0x0F, sizeof(screen_buffer));
    }

    void nes_t::clock()
    {
        if (ppu_clock_count % 3 == 0)
        {
            if (oam_dma.cycles_remaining == 0)
            {
                cpu.clock();
                if (cpu.cycles_until_next_instruction == 0)
                {
                    if (ppu.nmi)
                    {
                        cpu.nmi();
                        ppu.nmi = false;
                    }
                    else if (apu.dmc.irq || apu.frame_irq || (cart && cart->mapper->irq))
                    {
                        cpu.irq();
                        // IRQ will be cleared by the program
                    }
                }
            }
            oam_dma.clock();
            apu.clock();
        }
        ppu.clock();
        ppu_clock_count++;
        on_clock();
    }

    void nes_t::clock_instruction()
    {
        while (cpu.cycles_until_next_instruction == 0)
        {
            clock();
        }
        while (cpu.cycles_until_next_instruction > 0)
        {
            clock();
        }
    }

    void nes_t::clock_scanline()
    {
        for (int i = 0; i < ppu_t::DOTS_PER_SCANLINE; i++)
        {
            clock();
        }
    }

    void nes_t::clock_frame()
    {
        do
        {
            clock();
        } while (!(ppu.scanline == 0 && ppu.dot == 0));
    }

    void nes_t::unload_cart()
    {
        if (cart)
        {
            cpu_bus.disconnect_read(&*cart);
            cpu_bus.disconnect_write(&*cart);
            ppu_bus.disconnect_read(&*cart);
            ppu_bus.disconnect_write(&*cart);
            ppu.set_cart(nullptr);
            cart.reset();
        }
    }

    void nes_t::load_cart(std::unique_ptr<cart_t> cart)
    {
        unload_cart();
        this->cart = std::move(cart);
        cpu_bus.connect_read<&cart_t::cpu_read>(&*this->cart);
        cpu_bus.connect_write<&cart_t::cpu_write>(&*this->cart);
        ppu_bus.connect_read<&cart_t::ppu_read>(&*this->cart);
        ppu_bus.connect_write<&cart_t::ppu_write>(&*this->cart);
        ppu.set_cart(&*this->cart);
        reset();
    }
}
