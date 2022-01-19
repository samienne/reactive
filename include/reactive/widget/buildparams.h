#pragma once

#include "reactive/signal/sharedsignal.h"
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
        std::optional<AnySharedSignal<typename Tag::type>> get() const
        {
            auto r = params_.find(typeid(Tag));
            if (r == params_.end())
                return std::nullopt;

            return std::any_cast<AnySharedSignal<typename Tag::type>>(r->second);
        }

        template <typename Tag>
        void set(AnySharedSignal<typename Tag::type> value)
        {
            params_.insert_or_assign(typeid(Tag), std::move(value));
        }

    private:
        std::unordered_map<std::type_index, std::any> params_;
    };
} // namespace widget

