#pragma once
#include "pch.h"
#include <vector>

namespace nes
{
    struct bus_t
    {
        bus_t();

        template <auto /*bool (T::*)(uint16_t, uint8_t&) */ Read, typename T>
        void connect_read(T& obj)
        {
            readers.push_back(connection_t<bus_read_t>{
                .callback = [](uint16_t addr, uint8_t& value, void* ctx)
                {
                    T* typed_ctx = (T*)ctx;
                    return (typed_ctx->*Read)(addr, value);
                },
                .ctx = &obj,
            });
        }

        template <auto /*bool (T::*)(uint16_t, uint8_t) */ Write, typename T>
        void connect_write(T& obj)
        {
            writers.push_back(connection_t<bus_write_t>{
                .callback = [](uint16_t addr, uint8_t value, void* ctx)
                {
                    T* typed_ctx = (T*)ctx;
                    return (typed_ctx->*Write)(addr, value);
                },
                .ctx = &obj,
            });
        }

        void disconnect_read(void* obj);
        void disconnect_write(void* obj);

        uint8_t read(uint16_t addr);
        void write(uint16_t addr, uint8_t value);

    private:
        using bus_read_t = bool (*)(uint16_t addr, uint8_t& value, void* ctx);
        using bus_write_t = bool (*)(uint16_t addr, uint8_t value, void* ctx);
        
        template <typename Callback>
        struct connection_t
        {
            Callback callback;
            void* ctx;
        };
        std::vector<connection_t<bus_read_t>> readers;
        std::vector<connection_t<bus_write_t>> writers;
        uint8_t last_read;
    };
}