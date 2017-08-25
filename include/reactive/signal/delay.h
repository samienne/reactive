#pragma once

#include "reactive/signaltraits.h"
#include "reactive/signal/constant.h"

#include <btl/demangle.h>

namespace reactive
{
    namespace signal
    {
        template <typename T, typename U>
        class Delay
        {
        public:
            Delay(signal2::Signal<T, U>&& signal) :
                signal_(std::move(signal)),
                values_({
                        btl::just(btl::clone(signal_->evaluate())),
                        btl::none
                        })
            {
            }

            Delay(Delay&&) = default;
            Delay& operator=(Delay&&) = default;

            std::decay_t<T> const& evaluate() const
            {
                return *values_[index_];
            }

            bool hasChanged() const
            {
                return changed_;
            }

            UpdateResult updateBegin(signal::FrameInfo const& frame)
            {
                auto r = signal_->updateBegin(frame);

                changed_ = false;
                if (values_[(index_+1)%2].valid())
                {
                    index_ = (index_+1)%2;
                    changed_ = true;
                }

                if (changed_)
                    return btl::just(signal_time_t(0));

                return r;
            }

            UpdateResult updateEnd(signal::FrameInfo const& frame)
            {
                auto r = signal_->updateEnd(frame);
                if (signal_->hasChanged())
                    values_[(index_+1)%2] = btl::just(btl::clone(signal_->evaluate()));
                else
                    values_[(index_+1)%2] = btl::none;

                return r;
            }

            template <typename TCallback>
            Connection observe(TCallback&& callback)
            {
                return signal_->observe(std::forward<TCallback>(callback));
            }

            Annotation annotate() const
            {
                Annotation a;
                auto&& n = a.addNode("delay<" + btl::demangle<U>() + ">");
                a.addTree(n, signal_->annotate());
                return a;
            }

            Delay clone() const
            {
                return *this;
            }

        private:
            Delay(Delay const&) = default;
            Delay& operator=(Delay const&) = default;

        private:
            btl::CloneOnCopy<signal2::Signal<T, U>> signal_;
            btl::option<std::decay_t<T>> values_[2];
            uint8_t index_ = 0;
            bool changed_ = false;
        };

        static_assert(IsSignal<Delay<int const&, signal::Constant<int>>>::value,
                "");

        template <typename T, typename U>
        auto delay(signal2::Signal<T, U> sig)
        {
            return signal2::wrap(Delay<T, U>{ std::move(sig) });
        }
    } // signal
} // reactive

