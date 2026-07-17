#include "bqui/widget/datavalue.h"

#include <utility>

namespace bqui::widget
{

DataValue::DataValue(std::string value) : value(std::move(value)) {}
DataValue::DataValue(char const* value) : value(std::string(value)) {}
DataValue::DataValue(double value) : value(value) {}
DataValue::DataValue(bool value) : value(value) {}
DataValue::DataValue(DataMap value) : value(std::move(value)) {}
DataValue::DataValue(DataArray value) : value(std::move(value)) {}

bool operator==(DataValue const& lhs, DataValue const& rhs)
{
    return lhs.value == rhs.value;
}

bool operator!=(DataValue const& lhs, DataValue const& rhs)
{
    return !(lhs == rhs);
}

} // namespace bqui::widget
