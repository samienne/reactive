#pragma once

namespace btl
{
    class DummyLock
    {
    public:
        void lock()
        {
        }

        bool try_lock()
        {
            return true;
        }

        void unlock()
        {
        }
    };
} // btl

