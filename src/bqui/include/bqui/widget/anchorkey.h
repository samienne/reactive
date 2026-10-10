#pragma once

#include "bqui/bquivisibility.h"

#include <cstdint>

namespace bqui::widget
{
    /**
     * @brief The identity of an anchor key, as a band stores it.
     */
    using AnchorId = std::uint64_t;

    namespace detail
    {
        BQUI_EXPORT AnchorId newAnchorId();

        struct PredefinedAnchorId
        {
            AnchorId id;
        };

        struct XAnchorTag {};
        struct YAnchorTag {};
    } // namespace detail

    /**
     * @brief A key naming an anchor on one axis.
     *
     * A default-constructed key is new and unequal to every other; copies name
     * the same anchor. @p Tag keeps the axes apart, so a key of one axis cannot
     * publish or bind an anchor on the other.
     */
    template <typename Tag>
    class AnchorKey
    {
    public:
        AnchorKey() :
            id_(detail::newAnchorId())
        {
        }

        constexpr explicit AnchorKey(detail::PredefinedAnchorId predefined) :
            id_(predefined.id)
        {
        }

        constexpr AnchorId id() const
        {
            return id_;
        }

        constexpr bool operator==(AnchorKey const& other) const
        {
            return id_ == other.id_;
        }

        constexpr bool operator!=(AnchorKey const& other) const
        {
            return id_ != other.id_;
        }

    private:
        AnchorId id_;
    };

    using XAnchorKey = AnchorKey<detail::XAnchorTag>;
    using YAnchorKey = AnchorKey<detail::YAnchorTag>;

    /**
     * @brief The first baseline, published by text leaves and by rows that
     * align their children on it.
     */
    inline constexpr YAnchorKey baselineAnchor{ detail::PredefinedAnchorId{ 1 } };
} // namespace bqui::widget
