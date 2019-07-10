#pragma once

#include "single_pool_resource.h"
#include "memory_resource.h"

#include <memory>
#include <vector>
#include <cassert>

namespace pmr
{
    struct pool_options
    {
        std::size_t max_blocks_per_chunk;
        std::size_t largest_required_pool_block;
    };

    class unsynchronized_pool_resource : public memory_resource
    {
    public:
        explicit unsynchronized_pool_resource(memory_resource* upstream) :
            unsynchronized_pool_resource(pool_options{}, upstream)
        {
        }

        explicit unsynchronized_pool_resource(pool_options const& /*opts*/,
                memory_resource* upstream) :
            upstream_(upstream)
        {
            std::size_t smallest = 256;
            std::size_t step = 2;
            std::size_t largest = 256*1024;

            memory_resource* next_resource = upstream_;

            std::size_t s = largest;
            while (s >= smallest)
            {
                pools_.push_back(std::make_unique<single_pool_resource>(
                            s, step, next_resource
                            ));

                next_resource = pools_.back().get();
                s /= step;
            }
        }

        unsynchronized_pool_resource(
                unsynchronized_pool_resource const&) = delete;

    private:
        inline void* do_allocate(std::size_t size,
                std::size_t alignment) override
        {
#if 1
            // Calculate the correct pool from the size
            std::size_t const smallest = 256;

            int i = log2(size - 1) - (log2(smallest) - 2);
            i = pools_.size() - std::max(1, i);
            i = std::max(0, std::min(i, static_cast<int>(pools_.size() - 1)));

            return pools_[i]->allocate(size, alignment);
#else
            // Just use memory_resource upstream chaining.
            return pools_.back()->allocate(size, alignment);
#endif
        }

        inline void do_deallocate(void* p, std::size_t size,
                std::size_t alignment) override
        {
            pools_.back()->deallocate(p, size, alignment);
        }

        inline bool do_is_equal(memory_resource const& rhs) const override
        {
            return this == &rhs;
        }

        int log2(uint64_t n)
        {
            assert(n > 0);
#if 1
            int i = -(n==0);

            if (n >= (static_cast<uint64_t>(1ul) << 32)) { i += 32; n >>= 32; }
            if (n >= (static_cast<uint64_t>(1ul) << 16)) { i += 16; n >>= 16; }
            if (n >= (static_cast<uint64_t>(1ul) << 8)) { i += 8; n >>= 8; }
            if (n >= (static_cast<uint64_t>(1ul) << 4)) { i += 4; n >>= 4; }
            if (n >= (static_cast<uint64_t>(1ul) << 2)) { i += 2; n >>= 2; }
            if (n >= (static_cast<uint64_t>(1ul) << 1)) { i += 1; n >>= 1; }

            return i;
#else
            // Use gcc builtin
            return 63ul - __builtin_clzl(n);
#endif
        }

    private:
        memory_resource* upstream_ = nullptr;
        std::vector<std::unique_ptr<single_pool_resource>> pools_;
    };
} // namespace pmr

