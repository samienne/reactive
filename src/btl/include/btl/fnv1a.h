#pragma once

#include <cstddef>
#include <cstdint>

namespace btl
{
    class fnv1a
    {
    public:
        using result_type = uint64_t;

        void operator()(void const* key, std::size_t len) noexcept
        {
            unsigned char const* p = static_cast<unsigned char const*>(key);
            unsigned char const* const e = p + len;
            for (; p < e; ++p)
                state_ = (state_ ^ *p) * 1099511628211u;
        }

        explicit operator result_type() noexcept
        {
            return state_;
        }

    private:
        result_type state_ = 14695981039346656037u;
    };
}

