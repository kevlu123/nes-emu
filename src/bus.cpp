#include "pch.h"
#include "bus.h"

namespace nes
{
    bus_t::bus_t()
        : last_read(0xAA)
    {
    }

    void bus_t::disconnect_read(void* obj)
    {
        for (auto it = readers.begin(); it != readers.end();)
        {
            if (it->ctx == obj)
            {
                readers.erase(it);
                return;
            }
        }
        SPDLOG_ERROR("bus_t::disconnect_read failed");
    }

    void bus_t::disconnect_write(void* obj)
    {
        for (auto it = writers.begin(); it != writers.end();)
        {
            if (it->ctx == obj)
            {
                writers.erase(it);
                return;
            }
        }
        SPDLOG_ERROR("bus_t::disconnect_write failed");
    }

    uint8_t bus_t::read(uint16_t addr)
    {
        for (const auto& reader : readers)
        {
            if (reader.callback(addr, last_read, reader.ctx))
            {
                break;
            }
        }
        return last_read;
    }

    void bus_t::write(uint16_t addr, uint8_t value)
    {
        for (const auto& writer : writers)
        {
            if (writer.callback(addr, value, writer.ctx))
            {
                return;
            }
        }
    }
}
