#pragma once

#include "memory_resource.h"

#include <iostream>

namespace pmr
{
    class single_pool_resource : public memory_resource
    {
    public:
        inline single_pool_resource(std::size_t blockSize,
                std::size_t blockCount, memory_resource* upstream) :
            upstream_(upstream),
            blockSize_(blockSize),
            blockCount_(blockCount)
        {
        }

    private:
        inline void* do_allocate(std::size_t size, std::size_t alignment) override
        {
            if (size > blockSize_)
                return upstream_->allocate(size, alignment);

            if (!freelist_)
            {
                chunk* current = reinterpret_cast<chunk*>(
                        upstream_->allocate(blockSize_ * blockCount_)
                        );

                chunk* freelist = freelist_;

                for (std::size_t i = 0; i < blockCount_; ++i)
                {
                    current->next = freelist;
                    freelist = current;

                    current = reinterpret_cast<chunk*>(
                            reinterpret_cast<char*>(current) + blockSize_
                            );
                }

                freelist_ = freelist;
            }

            chunk* current = freelist_;
            freelist_ = current->next;

            return current;
        }

        inline void do_deallocate(void* p, std::size_t size,
                std::size_t alignment) override
        {
            if (size > blockSize_)
            {
                upstream_->deallocate(p, size, alignment);
                return;
            }

            chunk* c = reinterpret_cast<chunk*>(p);
            c->next = freelist_;

            freelist_ = c;
        }

        inline bool do_is_equal(memory_resource const& rhs) const override
        {
            return this == &rhs;
        }

        struct chunk
        {
            chunk* next;
        };

    private:
        memory_resource* upstream_ = nullptr;
        chunk* freelist_ = nullptr;
        std::size_t blockSize_;
        std::size_t blockCount_;
    };
} // namespace pmr

