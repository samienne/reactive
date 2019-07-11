#pragma once

#include <pmr/memory_resource.h>

#include <algorithm>
#include <cstring>

namespace btl
{
    class Buffer
    {
    public:
        inline explicit Buffer(pmr::memory_resource* memory) :
            memory_(memory)
        {
        }

        inline Buffer(Buffer const& rhs) :
            memory_(rhs.memory_)
        {
            if (rhs.data_)
            {
                data_ = memory_->allocate(rhs.reserved_);
                std::memcpy(data_, rhs.data_, rhs.size_);
            }

            reserved_ = rhs.reserved_;
            size_ = rhs.size_;
        }

        inline Buffer(Buffer&& rhs) noexcept :
            memory_(rhs.memory_),
            data_(rhs.data_),
            size_(rhs.size_),
            reserved_(rhs.reserved_)
        {
            rhs.data_ = nullptr;
            rhs.reserved_ = 0;
            rhs.size_ = 0;
        }

        inline ~Buffer()
        {
            if (data_)
                memory_->deallocate(data_, reserved_);
        }

        inline Buffer& operator=(Buffer const& rhs)
        {
            if (data_)
                memory_->deallocate(data_, reserved_);

            memory_ = rhs.memory_;

            if (rhs.data_)
            {
                data_ = memory_->allocate(rhs.reserved_);
                std::memcpy(data_, rhs.data_, rhs.size_);
            }

            reserved_ = rhs.reserved_;
            size_ = rhs.size_;

            return *this;
        }

        inline Buffer& operator=(Buffer&& rhs) noexcept
        {
            if (data_)
                memory_->deallocate(data_, reserved_);

            data_ = rhs.data_;
            reserved_ = rhs.reserved_;
            size_ = rhs.size_;

            rhs.data_ = nullptr;
            rhs.reserved_ = 0;
            rhs.size_ = 0;

            return *this;
        }

        inline void reserve(std::size_t size)
        {
            if (size <= reserved_)
                return;

            std::size_t newSize = std::max(128ul, reserved_ * 2);

            void* newData = memory_->allocate(newSize);
            memset(newData, 0, newSize);

            if (data_)
            {
                memcpy(newData, data_, size_);
                memory_->deallocate(data_, reserved_);
            }

            data_ = newData;
            reserved_ = newSize;
        }

        inline void pushData(void const* p, std::size_t size)
        {
            reserve(size_ + size);

            void* target = reinterpret_cast<char*>(data_) + size_;

            memcpy(target, p, size);
            size_ += size;
        }

        inline void* pop(std::size_t size)
        {
            size_ -= size;
            return reinterpret_cast<char*>(data_) + size_;
        }

        template <typename T>
        void push(T const& data)
        {
            pushData(&data, sizeof(T));
        }

        inline void* data()
        {
            return data_;
        }

        inline void const* data() const
        {
            return data_;
        }

        inline std::size_t size() const
        {
            return size_;
        }

        inline bool empty() const
        {
            return size_ == 0;
        }

        inline std::size_t reserved() const
        {
            return reserved_;
        }

        inline pmr::memory_resource* resource() const
        {
            return memory_;
        }

    private:
        pmr::memory_resource* memory_;
        void* data_ = nullptr;
        std::size_t size_ = 0;
        std::size_t reserved_ = 0;
    };

} // namespace btl

