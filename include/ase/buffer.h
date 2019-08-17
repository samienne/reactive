#pragma once

#include <btl/visibility.h>

#include <vector>
#include <memory>
#include <cstring>

namespace ase
{
    class BTL_VISIBLE BufferDeferred
    {
    public:
        inline BufferDeferred(size_t size) :
            len(size),
            data(size ? new char[size] : nullptr)
        {
        }

        inline ~BufferDeferred()
        {
            delete [] data;
        }

        size_t len;
        char* data;
    };

    /**
     * @brief Copy on write buffer
     */
    class BTL_VISIBLE Buffer
    {
    public:
        inline Buffer() :
            data_(nullptr),
            len_(0)
        {
        }

        /*
        inline Buffer(size_t len, void const* data = nullptr) :
            data_(nullptr),
            len_(len),
            deferred_(std::make_shared<BufferDeferred>(len))
        {
            if (data && len > 0)
                std::memcpy(deferred_->data, data, len);

            data_ = deferred_->data;
        }
        */

        inline Buffer(void const* data, size_t len) :
            data_(nullptr),
            len_(len),
            deferred_(std::make_shared<BufferDeferred>(len))
        {
            if (data)
                std::memcpy(deferred_->data, data, len);

            data_ = deferred_->data;
        }

        template <class T, class A>
        explicit inline Buffer(std::vector<T, A> const& data) :
            Buffer(data.data(), data.size() * sizeof(T))
        {
        }

        Buffer(Buffer const& rhs) = default;
        Buffer(Buffer&& rhs) = default;

        Buffer& operator=(Buffer const& rhs) = default;
        Buffer& operator=(Buffer&& rhs) = default;

        inline size_t getSize() const
        {
            return len_;
        }

        inline void const* data() const
        {
            return data_;
        }

        template<class T>
        inline T* mapWrite()
        {
            ensureUniqueness();
            return reinterpret_cast<T*>(deferred_->data);
        }

        template<class T>
        inline T const* mapRead() const
        {
            return reinterpret_cast<T const*>(deferred_->data);
        }

        inline char const* operator[](size_t index) const
        {
            return &data_[index];
        }

        inline char const* at(size_t index) const
        {
            if (index >= len_)
                throw std::runtime_error("Out of bounds");

            return &data_[index];
        }

        inline void resize(size_t newSize)
        {
            if (newSize == len_)
                return;

            auto newDeferred = std::make_shared<BufferDeferred>(newSize);
            if (data_)
                memcpy(newDeferred->data, data_, std::min(len_, newSize));

            deferred_ = newDeferred;

            data_ = deferred_->data;
            len_ = deferred_->len;
        }

    private:
        inline void ensureUniqueness()
        {
            if (deferred_.unique())
                return;

            auto own = std::make_shared<BufferDeferred>(deferred_->len);
            if (deferred_ && deferred_->data && deferred_->len > 0)
                memcpy(own->data, deferred_->data, deferred_->len);
        }

    private:
        char* data_;
        size_t len_;
        std::shared_ptr<BufferDeferred> deferred_;
    };
}

