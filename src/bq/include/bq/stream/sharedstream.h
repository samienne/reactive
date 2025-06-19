#pragma once

#include "stream.h"
#include "sharedcontrol.h"

namespace bq
{
    namespace stream
    {
        template <typename T>
        class SharedStream
        {
            template <typename TFunc>
            struct OnDelete
            {
                ~OnDelete()
                {
                    callBack();
                }

                TFunc callBack;
            };

        public:
            SharedStream(std::shared_ptr<SharedControl<T>> control) :
                control_(std::move(control))
            {
            }

            template <typename TFunc>
            auto fmap(TFunc&& f) const -> Stream<std::invoke_result_t<TFunc, T>>
            {
                using NewType = std::invoke_result_t<TFunc, T>;
                size_t index = nextIndex_++;

                auto destructor = [index, control = control_]()
                {
                    for (auto i = control->callbacks.begin();
                            i != control->callbacks.end(); ++i)
                    {
                        if (i->second == index)
                            control->callbacks.erase(i);
                    }
                };

                using NewControl = ControlWithData<NewType, decltype(destructor)>;
                auto newControl = std::make_shared<NewControl>(std::move(destructor));

                Handle<NewType> newHandle(newControl);

                auto callback =
                    [newHandle=std::move(newHandle), f=std::forward<TFunc>(f)](
                            T value)
                    {
                        newHandle.push(f(std::forward<T>(value)));
                    };

                control_->callbacks.push_back(
                        std::make_pair(std::move(callback), index));

                return Stream<NewType>(std::move(newControl));
            }

            operator Stream<T>() const
            {
                return fmap([](T value) { return std::forward<T>(value); });
            }

        private:
            std::shared_ptr<SharedControl<T>> control_;
            mutable size_t nextIndex_ = 1;
        };
    } // stream
} // reactive

