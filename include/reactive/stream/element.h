#pragma once

#include <btl/option.h>
#include <btl/spinlock.h>

#include <memory>

namespace reactive
{
    namespace stream
    {

    template <typename T>
    class Element
    {
    public:
        Element()
        {
        }

        Element(T&& value) :
            value_(std::move(value))
        {
        }

        Element(T const& value) :
            value_(value)
        {
        }

        void setNext(std::shared_ptr<Element> const& element)
        {
            std::unique_lock<btl::SpinLock> lock(spin_);
            next_ = element;
        }

        std::shared_ptr<Element<T>> const& getNext() const
        {
            std::unique_lock<btl::SpinLock> lock(spin_);
            return next_;
        }

        T const& getValue() const
        {
            assert(value_.valid());
            // Value never changes so no need for lock
            return *value_;
        }


    private:
        template <typename T2>
        friend class InputDeferred;

        std::shared_ptr<Element> next_;
        mutable btl::SpinLock spin_;
        btl::option<T> value_;
    };

    } // stream
} // reactive

