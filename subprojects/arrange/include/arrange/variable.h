#pragma once

#include <arrange/export.h>

#include <arrange/id.h>

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>

namespace arrange
{

class ARRANGE_API Variable
{
public:
    Variable();
    explicit Variable(std::string_view name);

    explicit Variable(Id id);
    Variable(Id id, std::string_view name);

    Id id() const noexcept;
    std::string_view name() const noexcept;

    bool operator==(const Variable& other) const noexcept;
    bool operator!=(const Variable& other) const noexcept;

private:
    Id id_{};
    std::shared_ptr<const std::string> name_{};
};

struct ARRANGE_API VariableHash
{
    std::size_t operator()(const Variable& v) const noexcept;
};

}  // namespace arrange
