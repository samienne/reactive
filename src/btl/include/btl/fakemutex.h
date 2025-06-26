#pragma once

namespace btl
{
    class fake_mutex
    {
    public:
        inline fake_mutex()
        {
        };

        fake_mutex(fake_mutex const&) = delete;
        fake_mutex(fake_mutex&&) = delete;
        fake_mutex& operator=(fake_mutex const&) = delete;
        fake_mutex& operator=(fake_mutex&&) = delete;

        inline void lock()
        {
        }

        inline void try_lock()
        {
        }

        inline void unlock()
        {
        }
    };
} // btl

