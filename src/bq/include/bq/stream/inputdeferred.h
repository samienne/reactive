#pragma once

#include "streambase.h"
#include "element.h"
#include "reactive/observable.h"

#include <btl/spinlock.h>

namespace bq
{
    namespace stream
    {

    template <typename T>
    class InputDeferred : public Observable
    {
    public:
        using Element = stream::Element<T>;

        InputDeferred()
        {
            last_ = std::make_shared<Element>();
            last_->next_ = std::make_shared<Element>();
        }

        InputDeferred(InputDeferred const&) = delete;
        InputDeferred& operator=(InputDeferred const&) = delete;

        virtual ~InputDeferred()
        {
        }

        std::shared_ptr<Element> getLast() const
        {
            std::unique_lock<btl::SpinLock> lock(spin_);
            return last_;
        }

        void push(T const& value)
        {
            {
                std::unique_lock<btl::SpinLock> lock(spin_);
                last_->value_ = std::make_optional(value);
                last_->next_->next_ = std::make_shared<Element>();
                last_ = last_->next_;
            }

            notify();
        }

    private:
        std::shared_ptr<Element> last_;
        mutable btl::SpinLock spin_;
    };

    } // stream
} // reactive

