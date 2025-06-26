#pragma once

#include "spinlock.h"

#include <stdint.h>
#include <ostream>

namespace btl
{
    class uid
    {
    public:
        inline bool operator==(uid const& rhs) const
        {
            return id_ == rhs.id_;
        }

        inline bool operator!=(uid const& rhs) const
        {
            return id_ != rhs.id_;
        }

        inline bool operator<(uid const& rhs) const
        {
            return id_ < rhs.id_;
        }

        inline bool operator>(uid const& rhs) const
        {
            return id_ > rhs.id_;
        }

        inline bool operator<=(uid const& rhs) const
        {
            return id_ <= rhs.id_;
        }

        inline bool operator>=(uid const& rhs) const
        {
            return id_ >= rhs.id_;
        }

        inline friend uid make_uid();

        inline friend std::ostream& operator<<(std::ostream& stream,
                uid const& id)
        {
            return stream << "btl::uid(" << id.id_ << ")";
        }

    private:
        inline uid(uint64_t id) :
            id_(id)
        {
        }

    private:
        uint64_t id_;
    };

    inline uint64_t get_uid_block()
    {
        static SpinLock spin;
        std::lock_guard<SpinLock> lock(spin);
        static uint64_t nextBlock = 1ul << 32;

        auto block = nextBlock;
        nextBlock += 1ul << 32;

        return block;
    }

    inline uid make_uid()
    {
        static thread_local uint64_t n = 0;

        if ((n % (1ul << 32)) == 0)
            n = get_uid_block();

        uint64_t id = n++;

        return uid(id);
    }
} // btl

