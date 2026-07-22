#pragma once

#include "bqui/bquivisibility.h"

#include <map>
#include <string>
#include <variant>
#include <vector>

namespace bqui::widget
{
    struct DataValue;

    /** @brief An ordered string-keyed bag of introspection data values. */
    using DataMap = std::map<std::string, DataValue>;

    /** @brief A sequence of introspection data values. */
    using DataArray = std::vector<DataValue>;

    /**
     * @brief A plain, JSON-shaped value carried in a widget's introspection data
     * bag.
     *
     * Holds a string, number, boolean, nested map, or array, so a consumer can
     * serialise a widget tree's declared data to JSON without depending on any
     * UI type. It is a value type with value semantics and equality, suitable
     * for carrying inside a reactive `AnySignal`.
     */
    struct BQUI_EXPORT DataValue
    {
        using Variant = std::variant<
            std::string,
            double,
            bool,
            DataMap,
            DataArray
            >;

        Variant value = std::string{};

        DataValue() = default;
        DataValue(std::string value);
        DataValue(char const* value);
        DataValue(double value);
        DataValue(bool value);
        DataValue(DataMap value);
        DataValue(DataArray value);
    };

    BQUI_EXPORT bool operator==(DataValue const& lhs, DataValue const& rhs);
    BQUI_EXPORT bool operator!=(DataValue const& lhs, DataValue const& rhs);
} // namespace bqui::widget
