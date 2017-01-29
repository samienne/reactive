#pragma once

#include <memory>
#include <cassert>

namespace btl
{
    template <typename T>
    class unique
    {
    public:
        unique(std::unique_ptr<T> rhs) :
            data_(std::move(rhs))
        {
            assert(data_ && "rhs cannot be empty");
        }

        unique(unique<T>&&) = default;

        ~unique()
        {
        }

        unique& operator=(unique<T>&&) = default;

        unique& operator=(std::unique_ptr<T>&& rhs)
        {
            assert(rhs && "rhs cannot be empty");
            data_ = std::move(rhs.data_);
            return *this;
        }

        T& operator*()
        {
            return *data_;
        }

        T const& operator*() const
        {
            assert(data_ && "use after move");
            return *data_;
        }

        T* operator->()
        {
            assert(data_ && "use after move");
            return data_.get();
        }

        T const* operator->() const
        {
            assert(data_ && "use after move");
            return data_.get();
        }

        operator std::unique_ptr<T>() &&
        {
            assert(data_ && "use after move");
            return std::move(data_);
        }

    private:
        std::unique_ptr<T> data_;
    };

    template <typename T, typename... TArgs>
    auto make_unique(TArgs&&... args) -> unique<T>
    {
        return unique<T>(
                std::make_unique<T>(
                    std::forward<TArgs>(args)...
                    )
                );
    }
} // btl

