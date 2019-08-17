#pragma once

#include "memory_resource.h"

#include <cstdint>
#include <algorithm>

namespace pmr
{
    class monotonic_buffer_resource : public memory_resource
    {
    public:
        inline explicit monotonic_buffer_resource(memory_resource* upstream) :
            monotonic_buffer_resource(4096, upstream)
        {
        }

        inline monotonic_buffer_resource(
                std::size_t initial_size,
                memory_resource* upstream) :
            upstream_(upstream),
            current_(reinterpret_cast<chunk*>(upstream->allocate(initial_size))),
            p_(reinterpret_cast<char*>(current_) + sizeof(chunk))
        {
            current_->prev = nullptr;
            current_->size = initial_size;
        }

        inline monotonic_buffer_resource(void* buffer, std::size_t buffer_size,
                memory_resource* upstream) :
            upstream_(upstream),
            current_(reinterpret_cast<chunk*>(buffer)),
            p_(reinterpret_cast<char*>(current_) + sizeof(chunk))
        {
            current_->prev = current_;
            current_->size = buffer_size;
        }

        inline ~monotonic_buffer_resource()
        {
            release();
        }

        inline void release()
        {
            chunk* ptr = current_;

            while (ptr && ptr->prev != ptr)
            {
                chunk* prev = ptr->prev;
                upstream_->deallocate(ptr, ptr->size);
                ptr = prev;
            }

            current_ = ptr;
            p_ = reinterpret_cast<char*>(current_) + sizeof(chunk);
        }

    private:
        inline void* do_allocate(std::size_t size, std::size_t alignment) override
        {
            // calculate offset for the alignment
            std::size_t offset =
                (alignment - (reinterpret_cast<std::uintptr_t>(p_) % alignment));

            if (offset == alignment)
                offset = 0;

            char* p = p_ + offset;

            if (!p || reinterpret_cast<char*>(current_) + current_->size
                    < p + size)
            {
                // Allocate new chunk
                std::size_t newSize = current_ ? current_->size * 2 : 4096;
                newSize = std::max(newSize, size + sizeof(chunk));

                chunk* newChunk = reinterpret_cast<chunk*>(
                        upstream_->allocate(newSize));
                newChunk->size = newSize;
                newChunk->prev = current_;

                current_ = newChunk;
                p = reinterpret_cast<char*>(current_) + sizeof(chunk);
            }

            p_ = p + size;

            return p;
        }

        inline void do_deallocate(void* /*p*/, std::size_t /*size*/,
                std::size_t /*alignment*/) override
        {
            // No op
        }

        inline bool do_is_equal(memory_resource const& rhs) const override
        {
            return this == &rhs;
        }

        struct chunk
        {
            chunk* prev;
            std::size_t size;
        };

        memory_resource* upstream_ = nullptr;
        chunk* current_ = nullptr;
        char* p_ = nullptr;
    };
} // namespace pmr

