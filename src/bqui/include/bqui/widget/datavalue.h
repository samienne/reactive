#pragma once

#include <map>
#include <string>
#include <variant>
#include <vector>

namespace bqui::widget
{
    struct DataValue;

    /// An ordered string-keyed bag of introspection data values.
    using DataMap = std::map<std::string, DataValue>;

    /// A sequence of introspection data values.
    using DataArray = std::vector<DataValue>;

    /// A plain, JSON-shaped value carried in a widget's introspection data bag.
    ///
    /// Holds a string, number, boolean, nested map, or array, so a consumer can
    /// serialise a widget tree's declared data to JSON without depending on any
    /// UI type. It is a value type with value semantics and equality, suitable
    /// for carrying inside a reactive `AnySignal`.
    struct DataValue
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
        DataValue(std::string value) : value(std::move(value)) {}
        DataValue(char const* value) : value(std::string(value)) {}
        DataValue(double value) : value(value) {}
        DataValue(bool value) : value(value) {}
        DataValue(DataMap value) : value(std::move(value)) {}
        DataValue(DataArray value) : value(std::move(value)) {}
    };

    inline bool operator==(DataValue const& lhs, DataValue const& rhs)
    {
        return lhs.value == rhs.value;
    }

    inline bool operator!=(DataValue const& lhs, DataValue const& rhs)
    {
        return !(lhs == rhs);
    }
} // namespace bqui::widget
