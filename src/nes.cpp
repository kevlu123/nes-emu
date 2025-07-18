
#include "pch.h"
#include "nes.h"

namespace nes
{
    nes_t::nes_t()
        : cpu(cpu_bus),
          ppu(ppu_bus, cpu, screen_buffer),
          ppu_clock_count(0),
          screen_buffer{}
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
        controller.reset();
        if (cart)
        {
            cart->reset();
        }
    }

    void nes_t::clock()
    {
        if (ppu_clock_count % 3 == 0)
        {
            cpu.clock();
        }
        ppu.clock();
        ppu_clock_count++;
    }

    void nes_t::clock_frame()
    {
        do
        {
            clock();
        } while (ppu.scanline != 0 || ppu.dot != 0);
    }

    void nes_t::unload_cart()
    {
        if (cart)
        {
            cpu_bus.disconnect_read(&*cart);
            cpu_bus.disconnect_write(&*cart);
            ppu_bus.disconnect_read(&*cart);
            ppu_bus.disconnect_write(&*cart);
            cart = std::nullopt;
        }
    }

    void nes_t::load_cart(cart_t cart)
    {
        unload_cart();
        this->cart = std::move(cart);
        cpu_bus.connect_read<&cart_t::read>(&*this->cart);
        cpu_bus.connect_write<&cart_t::write>(&*this->cart);
        ppu_bus.connect_read<&cart_t::read>(&*this->cart);
        ppu_bus.connect_write<&cart_t::write>(&*this->cart);
        reset();
    }
}
