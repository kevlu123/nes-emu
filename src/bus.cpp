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
        for (auto it = readers.begin(); it != readers.end(); ++it)
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
        for (auto it = writers.begin(); it != writers.end(); ++it)
        {
            if (it->ctx == obj)
            {
                writers.erase(it);
                return;
            }
        }
        SPDLOG_ERROR("bus_t::disconnect_write failed");
    }

    uint8_t bus_t::read(uint16_t addr, bool readonly)
    {
        for (const auto& reader : readers)
        {
            if (reader.callback(addr, last_read, readonly, reader.ctx))
            {
                return last_read;
            }
        }
        SPDLOG_WARN("No read handler *{:04X}", addr);
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
        SPDLOG_WARN("No write handler *{:04X} = {:02X}", addr, value);
    }
}
