#pragma once

#include "stream.h"
#include "sharedcontrol.h"

#include <optional>
#include <utility>

namespace bq
{
    namespace stream
    {
        template <typename T>
        class SharedStream
        {
            // Move-only, fire-once RAII guard: runs its function exactly once,
            // when the still-active instance is destroyed. Moving transfers the
            // function, disarming the moved-from instance so that the temporary
            // spun up while constructing the downstream control does not fire
            // the unregister early (before the callback is even registered) or a
            // second time.
            template <typename TFunc>
            struct OnDelete
            {
                explicit OnDelete(TFunc callBack) :
                    callBack(std::move(callBack))
                {
                }

                OnDelete(OnDelete&& other) noexcept :
                    callBack(std::exchange(other.callBack, std::nullopt))
                {
                }

                OnDelete(OnDelete const&) = delete;
                OnDelete& operator=(OnDelete&&) = delete;
                OnDelete& operator=(OnDelete const&) = delete;

                ~OnDelete()
                {
                    if (callBack)
                        (*callBack)();
                }

                std::optional<TFunc> callBack;
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
                size_t index = control_->nextIndex++;

                auto unregister = [index, control = control_]()
                {
                    for (auto i = control->callbacks.begin();
                            i != control->callbacks.end(); ++i)
                    {
                        if (i->second == index)
                        {
                            control->callbacks.erase(i);
                            break;
                        }
                    }
                };

                OnDelete<decltype(unregister)> destructor(std::move(unregister));

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
        };
    } // stream
} // reactive

