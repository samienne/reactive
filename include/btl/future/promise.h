#pragma once

#include "futurebase.h"

#include <memory>

namespace btl
{
    namespace future
    {
        template <typename T>
        class Promise
        {
        public:
            Promise(std::weak_ptr<FutureControl<T>> control) :
                control_(std::move(control))
            {
            }

            bool valid() const
            {
                return !control_.expired();
            }

            void set(T value)
            {
                if (auto p = control_.lock())
                    p->set(std::forward<T>(value));
            }

        private:
            std::weak_ptr<FutureControl<T>> control_;
        };
    } // future
} // btl

