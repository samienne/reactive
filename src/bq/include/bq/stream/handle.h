#pragma once

#include "control.h"

#include <memory>

namespace bq
{
    namespace stream
    {
        template <typename T>
        class Handle
        {
        public:
            Handle(std::weak_ptr<Control<T>> control) :
                control_(std::move(control))
            {
            }

            void push(T value) const
            {
                if (auto p = control_.lock())
                {
                    if (p->callback)
                        p->callback(std::forward<T>(value));
                }
            }

        private:
            std::weak_ptr<Control<T>> control_;
        };
    } // stream
} // reactive

