#pragma once

#include <pmr/memory_resource.h>

#include <vector>
#include <iostream>
#include <cassert>

namespace avg
{

class TraceResource : public pmr::memory_resource
{
public:
    TraceResource(pmr::memory_resource* upstream) :
        upstream_(upstream)
    {
    }

    ~TraceResource()
    {
        assert(allocations_.empty());
    }

    std::size_t getAllocationCount() const
    {
        return allocationCount_;
    }

    std::size_t getTotalAllocation() const
    {
        return totalAllocation_;
    }

    std::size_t getCurrentAllocation() const
    {
        return currentAllocation_;
    }

    std::size_t getMaxAllocation() const
    {
        return maxAllocation_;
    }

    std::size_t getConsecutiveAllocDealloc() const
    {
        return consecutiveAllocDealloc_;
    }

    std::size_t getConsecutiveAllocDeallocBytes() const
    {
        return consecutiveAllocDeallocBytes_;
    }

private:
    void* do_allocate(std::size_t size, std::size_t alignment) override
    {
        totalAllocation_ += size;
        allocationCount_ += 1;
        currentAllocation_ += size;

        maxAllocation_ = std::max(currentAllocation_, maxAllocation_);

        void* p = upstream_->allocate(size, alignment);
        allocations_.push_back(Allocation{ p, size, alignment });

        previous_ = p;

        return p;
    }

    void do_deallocate(void* p, std::size_t size, std::size_t alignment) override
    {
        if (p == previous_)
        {
            consecutiveAllocDealloc_ += 1;
            consecutiveAllocDeallocBytes_ += size;
        }

        currentAllocation_ -= size;

        bool found = false;
        for (auto i = allocations_.begin(); i != allocations_.end(); ++i)
        {
            Allocation& a = *i;
            if (a.p == p)
            {
                if (a.size != size)
                {
                    std::cout << "size mismatch: " << a.size << " != " << size
                        << std::endl;
                }

                if (a.alignment != alignment)
                {
                    std::cout << "alignment mismatch: "
                        << a.alignment << " != " << alignment << std::endl;
                }

                assert(a.size == size && a.alignment == alignment);
                found = true;
                allocations_.erase(i);
                break;
            }
        }

        //assert(found);
        if (!found)
        {
            std::cout << "didn't find allocation " << p << std::endl;
        }

        return upstream_->deallocate(p, size, alignment);
    }

    bool do_is_equal(memory_resource const& rhs) const override
    {
        return this == &rhs;
    }

    memory_resource* upstream_;

    struct Allocation
    {
        void* p;
        std::size_t size;
        std::size_t alignment;
    };

    std::vector<Allocation> allocations_;
    std::size_t allocationCount_ = 0;
    std::size_t totalAllocation_ = 0;
    std::size_t currentAllocation_ = 0;
    std::size_t maxAllocation_ = 0;
    std::size_t consecutiveAllocDealloc_ = 0;
    std::size_t consecutiveAllocDeallocBytes_ = 0;
    void* previous_ = nullptr;
};

} // namespace avg

