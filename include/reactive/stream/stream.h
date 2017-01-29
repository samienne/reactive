#pragma once

#include "control.h"
#include "handle.h"
#include "sharedcontrol.h"

#include <memory>

namespace reactive
{
    namespace stream
    {
        template <typename T>
        class SharedStream;

        template <typename T>
        class Stream
        {
        public:
            Stream(std::shared_ptr<Control<T>> control) :
                control_(std::move(control))
            {
            }

            /*
            Stream(Stream const&) = delete;
            Stream(Stream&&) noexcept = default;
            Stream& operator=(Stream const&) = delete;
            Stream& operator=(Stream&&) noexcept = default;
            */

            template <typename TFunc>
            auto fmap(TFunc&& f) && -> Stream<std::result_of_t<TFunc(T)>>
            {
                using NewType = std::result_of_t<TFunc(T)>;
                using NewControlType = ControlWithData<NewType, decltype(control_)>;
                auto newControl = std::make_shared<NewControlType>(std::move(control_));
                Handle<NewType> newHandle(newControl);

                newControl->data->callback =
                    [newHandle=std::move(newHandle), f=std::forward<TFunc>(f)](
                            T value)
                    {
                        newHandle.push(f(std::forward<T>(value)));
                    };

                return Stream<NewType>(std::move(newControl));
            }

            SharedStream<T> share() &&
            {
                auto newControl = std::make_shared<SharedControl<T>>();
                std::weak_ptr<SharedControl<T>> weakControl(newControl);

                control_->callback =
                    [control = std::move(weakControl)](T value)
                    {
                        if (auto p = control.lock())
                        {
                            auto callbacks = p->callbacks;
                            for (auto&& cb : callbacks)
                                cb.first(value);
                        }
                    };

                return SharedStream<T>(std::move(newControl));
            }

        private:
            std::shared_ptr<Control<T>> control_;
        };
    } // stream
} // reactive

