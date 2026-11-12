#include "uniformvalues.h"

#include "uniformblock.h"

#include <cstring>
#include <stdexcept>
#include <string>

namespace ase
{

namespace
{
    constexpr size_t vec4Size = 16;
    constexpr size_t componentSize = 4;
} // anonymous namespace

UniformValues::UniformValues(ShaderInterface const& iface,
        std::string_view blockName) :
    UniformValues(findDeclaredBlock(iface, blockName))
{
}

UniformValues::UniformValues(ShaderInterface::Block block) :
    block_(std::move(block)),
    bytes_(block_.size, 0)
{
}

ShaderInterface::Block const& UniformValues::getBlock() const
{
    return block_;
}

void const* UniformValues::data() const
{
    return bytes_.data();
}

size_t UniformValues::getSize() const
{
    return bytes_.size();
}

ShaderInterface::Member const& UniformValues::resolve(std::string_view name,
        UniformType type, std::optional<size_t> arrayCount) const
{
    ShaderInterface::Member const* member = block_.findMember(name);
    if (!member)
    {
        throw std::invalid_argument("Block '" + block_.name
                + "' has no member '" + std::string(name) + "'");
    }

    std::string const qualified = block_.name + "." + member->name;

    if (member->type != type)
    {
        throw std::invalid_argument("Member '" + qualified + "' is "
                + std::string(toString(member->type)) + ", not "
                + std::string(toString(type)));
    }

    if (arrayCount.has_value() != (member->arraySize > 0))
    {
        throw std::invalid_argument("Member '" + qualified + "' is "
                + (arrayCount ? "not an array" : "an array"));
    }

    if (arrayCount && *arrayCount != static_cast<size_t>(member->arraySize))
    {
        throw std::invalid_argument("Member '" + qualified + "' has "
                + std::to_string(member->arraySize) + " elements, "
                + std::to_string(*arrayCount) + " given");
    }

    return *member;
}

void UniformValues::writeElement(ShaderInterface::Member const& member,
        size_t index, void const* element)
{
    char* target = bytes_.data() + member.offset + index * member.arrayStride;
    char const* source = static_cast<char const*>(element);
    size_t const n = static_cast<size_t>(componentCount(member.type));

    if (isMatrix(member.type))
    {
        for (size_t column = 0; column < n; ++column)
        {
            std::memcpy(target + column * vec4Size,
                    source + column * n * componentSize, n * componentSize);
        }
    }
    else
    {
        std::memcpy(target, source, n * componentSize);
    }
}

} // namespace ase
