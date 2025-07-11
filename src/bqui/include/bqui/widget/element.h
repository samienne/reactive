#pragma once

#include "instance.h"

#include "bqui/buildparams.h"

#include <avg/vector.h>

#include <btl/cloneoncopy.h>

namespace bqui::widget
{
    template <typename TInstance>
    class Element;

    using AnyElement = Element<bq::signal::AnySignal<Instance>>;

    template <typename TInstance>
    auto makeElement(bq::signal::Signal<TInstance, Instance> instance, BuildParams params);

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

        Element setParams(BuildParams params) &&
        {
            return makeElement(
                    std::move(*instance_),
                    std::move(params)
                    );
        }

        BuildParams const& getParams() const
        {
            return params_;
        }

        operator AnyElement() &&
        {
            return AnyElement(std::move(*instance_), std::move(params_));
        }

        auto share() &&
        {
            return makeElement(
                    std::move(*instance_).share(),
                    std::move(params_)
                    );
        }

        auto getSize() const
        {
            return instance_->clone().map(&Instance::getSize);
        }

        auto getRenderTree() const
        {
            return instance_->clone().map(&Instance::getRenderTree);
        }

        auto getObb() const
        {
            return instance_->clone().map(&Instance::getObb);
        }

        auto getInputAreas() const
        {
            return instance_->clone().map(&Instance::getInputAreas);
        }

        auto getKeyboardInputs() const
        {
            return instance_->clone().map(&Instance::getKeyboardInputs);
        }

    private:
        btl::CloneOnCopy<TInstance> instance_;
        BuildParams params_;
    };

    extern template class BQUI_EXPORT_TEMPLATE
        Element<bq::signal::AnySignal<Instance>>;

    template <typename TInstance>
    auto makeElement(bq::signal::Signal<TInstance, Instance> instance, BuildParams params)
    {
        return Element<bq::signal::Signal<TInstance, Instance>>(
                std::move(instance),
                std::move(params)
                );
    }

    template <typename T>
    auto makeElement(bq::signal::Signal<T, avg::Vector2f> size)
    {
        return makeElement(
                makeInstance(std::move(size)),
                BuildParams()
                );
    }
} // namespace bqui::widget

