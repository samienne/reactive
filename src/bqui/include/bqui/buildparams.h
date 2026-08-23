#pragma once

#include "bquivisibility.h"

#include <bq/signal/signal.h>

#include <memory>
#include <optional>
#include <string>
#include <typeinfo>
#include <unordered_map>

namespace bqui
{
    /** @brief A type-keyed bag of inherited build-time parameters.
     *
     * Entries are keyed on the tag type's mangled name rather than
     * `typeid(Tag)` directly: a type's `type_info` is emitted per binary under
     * hidden symbol visibility and can compare unequal across a shared-library
     * boundary (observed on macOS), which would make a parameter set in one
     * binary invisible to a reader in another. The mangled name is identical
     * across binaries, and the stored value is recovered by a trusted
     * `static_pointer_cast` keyed on that name, so no cross-binary RTTI identity
     * is relied on in either the lookup or the cast.
     */
    class BQUI_EXPORT BuildParams
    {
    public:
        BuildParams() = default;
        BuildParams(BuildParams const&) = default;
        BuildParams(BuildParams&&) noexcept = default;

        BuildParams& operator=(BuildParams const&) = default;
        BuildParams& operator=(BuildParams&&) noexcept = default;

        template <typename Tag>
        std::optional<bq::signal::AnySignal<typename Tag::type>> get() const
        {
            auto r = params_.find(key<Tag>());
            if (r == params_.end())
                return std::nullopt;

            return *std::static_pointer_cast<
                bq::signal::AnySignal<typename Tag::type>>(r->second);
        }

        template <typename Tag>
        bq::signal::AnySignal<typename Tag::type> valueOrDefault() const
        {
            auto value = get<Tag>();
            if (!value)
                return Tag::getDefaultValue();

            return *value;
        }

        template <typename Tag, typename T>
        void set(bq::signal::Signal<T, typename Tag::type> value)
        {
            params_.insert_or_assign(
                    key<Tag>(),
                    std::static_pointer_cast<void>(
                        std::make_shared<
                            bq::signal::AnySignal<typename Tag::type>>(
                                std::move(value)))
                    );
        }

        template <typename Tag>
        void set(typename Tag::type value)
        {
            set(share(bq::signal::constant<typename Tag::type>(std::move(value))));
        }

        size_t size() const
        {
            return params_.size();
        }

    private:
        template <typename Tag>
        static std::string key()
        {
            return typeid(Tag).name();
        }

        std::unordered_map<std::string, std::shared_ptr<void>> params_;
    };
}
