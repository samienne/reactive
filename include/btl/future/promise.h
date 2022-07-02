#pragma once

#include "futurecontrol.h"
#include "futurebase.h"

#include <memory>

namespace btl::future
{
    template <typename... Ts>
    class Promise
    {
    public:
        Promise(std::weak_ptr<FutureControl<Ts...>> control) :
            control_(std::move(control))
        {
        }

        bool valid() const
        {
            return !control_.expired();
        }

        void set(Ts... values)
        {
            if (auto p = control_.lock())
                p->set(std::forward<Ts>(values)...);
        }

        void setFromTuple(std::tuple<Ts...> values)
        {
            if (auto p = control_.lock())
                p->set(std::move(values));
        }

    private:
        std::weak_ptr<FutureControl<Ts...>> control_;
    };
} // namespace btl::future

