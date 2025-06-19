#pragma once

#include "memory_resource.h"

#include <algorithm>

namespace pmr
{
    class statistics_resource : public memory_resource
    {
    public:
        explicit statistics_resource(memory_resource* upstream) :
            upstream_(upstream)
        {
        }

        inline bool do_is_equal(memory_resource const& rhs) const override
        {
            return this == &rhs;
        }

        inline size_t maximum_concurrent_bytes_allocated() const
        {
            return maximumConcurrentBytesAllocated_;
        }

        inline size_t bytes_allocated() const
        {
            return bytesAllocated_;
        }

        inline size_t allocation_count() const
        {
            return allocationCount_;
        }

        inline size_t total_bytes_allocated() const
        {
            return totalBytesAllocated_;
        }

        inline size_t total_allocation_count() const
        {
            return totalAllocationCount_;
        }

    private:
        inline void* do_allocate(std::size_t size, std::size_t alignment) override
        {
            bytesAllocated_ += size;
            allocationCount_ += 1;
            totalBytesAllocated_ += size;
            totalAllocationCount_ += 1;

            maximumConcurrentBytesAllocated_ = std::max(
                    maximumConcurrentBytesAllocated_, bytesAllocated_);

            return upstream_->allocate(size, alignment);
        }

        inline void do_deallocate(void* p, std::size_t size,
                std::size_t alignment) override
        {
            bytesAllocated_ -= size;
            allocationCount_ -= 1;

            upstream_->deallocate(p, size, alignment);
        }

    private:
        memory_resource* upstream_;

        size_t maximumConcurrentBytesAllocated_ = 0u;
        size_t bytesAllocated_ = 0u;
        size_t allocationCount_ = 0u;
        size_t totalBytesAllocated_ = 0u;
        size_t totalAllocationCount_ = 0u;
    };
} // namespace pmr

