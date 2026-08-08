#include "id_counter.h"

#include <arrange/errors.h>
#include <arrange/variable.h>

#include <functional>
#include <string>
#include <string_view>
#include <utility>

namespace arrange
{

namespace
{

constexpr Id kUserIdMask = Id{1} << 63;

void validateUserId(Id id)
{
    if (id == nullId)
    {
        throw BadId{"Variable id must not be nullId"};
    }
    if ((id & kUserIdMask) != 0)
    {
        throw BadId{"Variable user id must not have the high bit set"};
    }
}

std::shared_ptr<const std::string> makeNameOrNull(std::string_view name)
{
    if (name.empty())
    {
        return nullptr;
    }
    return std::make_shared<const std::string>(name);
}

}  // namespace

Variable::Variable()
    : id_(detail::nextAutoVariableId())
    , name_(nullptr)
{
}

Variable::Variable(std::string_view name)
    : id_(detail::nextAutoVariableId())
    , name_(makeNameOrNull(name))
{
}

Variable::Variable(Id id)
    : id_(id)
    , name_(nullptr)
{
    validateUserId(id);
}

Variable::Variable(Id id, std::string_view name)
    : id_(id)
    , name_(makeNameOrNull(name))
{
    validateUserId(id);
}

Id Variable::id() const noexcept
{
    return id_;
}

std::string_view Variable::name() const noexcept
{
    if (!name_)
    {
        return {};
    }
    return *name_;
}

bool Variable::operator==(const Variable& other) const noexcept
{
    return id_ == other.id_;
}

bool Variable::operator!=(const Variable& other) const noexcept
{
    return id_ != other.id_;
}

std::size_t VariableHash::operator()(const Variable& v) const noexcept
{
    return std::hash<Id>{}(v.id());
}

}  // namespace arrange
