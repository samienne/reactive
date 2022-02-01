#pragma once

#include "btl/cloneoncopy.h"
#include "instance.h"
#include "buildparams.h"

#include <avg/vector.h>

namespace reactive::widget
{
    template <typename TInstance>
    class Element;

    using AnyElement = Element<AnySignal<Instance>>;

    template <typename TInstance>
    class Element
    {
    public:
        Element(TInstance instance, BuildParams params) :
            instance_(std::move(instance)),
            params_(std::move(params))
        {
        }

        TInstance&& getInstance() &&
        {
            return std::move(*instance_);
        }

        BuildParams const& getParams() const
        {
            return params_;
        }

        operator AnyElement() &&
        {
            return AnyElement(std::move(*instance_), std::move(params_));
        }

    private:
        btl::CloneOnCopy<TInstance> instance_;
        BuildParams params_;
    };

    template <typename TInstance>
    auto makeElement(Signal<TInstance, Instance> instance, BuildParams params)
    {
        return Element<Signal<TInstance, Instance>>(
                std::move(instance),
                std::move(params)
                );
    }

    template <typename T>
    auto makeElement(Signal<T, avg::Vector2f> size)
    {
        return makeElement(
                makeInstance(std::move(size)),
                BuildParams()
                );
    }
} // namespace reactive::widget

