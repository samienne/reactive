#pragma once

#include "reactive/signal/signal.h"

#include <any>
#include <typeindex>
#include <optional>
#include <unordered_map>

namespace reactive::widget
{
    class BuildParams
    {
    public:
        BuildParams() = default;
        BuildParams(BuildParams const&) = default;
        BuildParams(BuildParams&&) noexcept = default;

        BuildParams& operator=(BuildParams const&) = default;
        BuildParams& operator=(BuildParams&&) noexcept = default;

        template <typename Tag>
        std::optional<signal::AnySignal<typename Tag::type>> get() const
        {
            auto r = params_.find(typeid(Tag));
            if (r == params_.end())
                return std::nullopt;

            return std::any_cast<signal::AnySignal<typename Tag::type>>(r->second);
        }

        template <typename Tag>
        signal::AnySignal<typename Tag::type> valueOrDefault() const
        {
            auto value = get<Tag>();
            if (!value)
                return Tag::getDefaultValue();

            return *value;
        }

        template <typename Tag, typename T>
        void set(signal::Signal<T, typename Tag::type> value)
        {
            params_.insert_or_assign(
                    typeid(Tag),
                    signal::AnySignal<typename Tag::type>(std::move(value))
                    );
        }

        template <typename Tag>
        void set(typename Tag::type value)
        {
            set(share(signal::constant<typename Tag::type>(std::move(value))));
        }

    private:
        std::unordered_map<std::type_index, std::any> params_;
    };
} // namespace widget

