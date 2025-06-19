#pragma once

#include "vector.h"

#include "memory_resource.h"

#include <cassert>

namespace pmr
{
    class auto_release_resource : public memory_resource
    {
    public:
        explicit auto_release_resource(memory_resource* upstream) :
            upstream_(upstream),
            allocations_(upstream)
        {
        }

        ~auto_release_resource()
        {
            for (auto const& a : allocations_)
                upstream_->deallocate(a.p, a.size, a.alignment);
        }

    private:
        inline void* do_allocate(std::size_t size,
                std::size_t alignment) override
        {
            void* p = upstream_->allocate(size, alignment);

            try
            {
                allocations_.push_back(allocation{ p, size, alignment });
            }
            catch (...)
            {
                upstream_->deallocate(p, size, alignment);
                throw;
            }

            return p;
        }

        inline void do_deallocate(void* p, std::size_t size,
                std::size_t alignment) override
        {
            for (auto i = allocations_.begin(); i != allocations_.end(); ++i)
            {
                if (i->p == p)
                {
                    assert(i->size == size);
                    assert(i->alignment == alignment);

                    allocations_.erase(i);
                    break;
                }
            }
        }

        inline bool do_is_equal(memory_resource const& rhs) const override
        {
            return this == &rhs;
        }

    private:
        memory_resource* upstream_;

        struct allocation
        {
            void* p;
            size_t size;
            size_t alignment;
        };

        pmr::vector<allocation> allocations_;
    };
} // namespace pmr

