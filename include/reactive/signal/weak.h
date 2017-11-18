#pragma once

#include "reactive/signal.h"
#include "reactive/signaltraits.h"

#include <btl/hidden.h>

BTL_VISIBILITY_PUSH_HIDDEN

namespace reactive
{
    namespace signal
    {
        template <typename T>
        class BTL_CLASS_VISIBLE Weak
        {
        public:
            BTL_HIDDEN Weak()
            {
            }

            BTL_HIDDEN Weak(std::weak_ptr<SignalBase<T>> sig) :
                deferred_(std::move(sig))
            {
            }

            template <typename U>
            BTL_HIDDEN Weak(SharedSignal<T, U> const& sig) :
                deferred_(sig.signal().weak())
            {
            }

            BTL_HIDDEN auto evaluate() const
                -> decltype(std::declval<SignalBase<T>>().evaluate())
            {
                auto p = deferred_.lock();
                assert(p);

                return p->evaluate();
            }

            BTL_HIDDEN bool hasChanged() const
            {
                if (auto p = deferred_.lock())
                    return p->hasChanged();
                else
                    return false;
            }

            BTL_HIDDEN UpdateResult updateBegin(FrameInfo const& frame)
            {
                if (auto p = deferred_.lock())
                    return p->updateBegin(frame);
                else
                    return btl::none;
            }

            BTL_HIDDEN UpdateResult updateEnd(FrameInfo const& frame)
            {
                if (auto p = deferred_.lock())
                    return p->updateEnd(frame);
                else
                    return btl::none;
            }

            template <typename TCallback>
            BTL_HIDDEN Connection observe(TCallback&& callback)
            {
                if (auto p = deferred_.lock())
                    return p->observe(std::forward<TCallback>(callback));
                else
                    return Connection();
            }

            BTL_HIDDEN Annotation annotate() const
            {
                if (auto p = deferred_.lock())
                {
                    Annotation a;
                    auto&& n = a.addNode("weak()");
                    a.addShared(p.get(), n, p->annotate());
                    return a;
                }

                Annotation a;
                a.addNode("weak(disconnected)");
                return a;
            }

            BTL_HIDDEN Weak clone() const
            {
                return *this;
            }

            BTL_HIDDEN bool operator==(Weak const& rhs) const
            {
                return deferred_.lock().get() == rhs.deferred_.lock().get();
            }

            BTL_HIDDEN bool operator!=(Weak const& rhs) const
            {
                return !(*this == rhs);
            }

            BTL_HIDDEN bool isValid() const
            {
                return !deferred_.expired();
            }

        private:
            std::weak_ptr<SignalBase<T>> deferred_;
        };

        template <typename T, typename U>
        auto weak(SharedSignal<T, U> const& sig) // -> Weak<T>
        {
            return signal::wrap(Weak<T>(sig));
        }
    }
}

BTL_VISIBILITY_POP

