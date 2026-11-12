#include "shaderinterface.h"

#include <stdexcept>

namespace ase
{

namespace
{
    struct Std140Type
    {
        size_t size;
        size_t alignment;
    };

    constexpr size_t vec4Size = 16;

    size_t roundUp(size_t value, size_t alignment)
    {
        return (value + alignment - 1) / alignment * alignment;
    }
} // anonymous namespace

int componentCount(UniformType type)
{
    switch (type)
    {
        case UniformType::Float:
        case UniformType::Int:
        case UniformType::UInt:
            return 1;
        case UniformType::Float2:
        case UniformType::Int2:
        case UniformType::UInt2:
        case UniformType::Mat2:
            return 2;
        case UniformType::Float3:
        case UniformType::Int3:
        case UniformType::UInt3:
        case UniformType::Mat3:
            return 3;
        case UniformType::Float4:
        case UniformType::Int4:
        case UniformType::UInt4:
        case UniformType::Mat4:
            return 4;
    }

    return 0;
}

bool isMatrix(UniformType type)
{
    return type == UniformType::Mat2 || type == UniformType::Mat3
        || type == UniformType::Mat4;
}

namespace
{
    // A matrix column is an array element in std140, so it is padded to vec4.
    Std140Type std140Type(UniformType type)
    {
        size_t const n = static_cast<size_t>(componentCount(type));

        if (isMatrix(type))
            return { n * vec4Size, vec4Size };

        size_t const size = n * 4;
        size_t const alignment = n == 3 ? vec4Size : size;
        return { size, alignment };
    }

    template <class T>
    void requireUnique(std::vector<T> const& entries, std::string const& name,
            int slot, char const* kind, char const* slotName,
            int T::*slotMember)
    {
        if (slot < 0)
        {
            throw std::invalid_argument(std::string(kind) + " '" + name
                    + "' has a negative " + slotName);
        }

        for (auto const& entry : entries)
        {
            if (entry.name == name)
            {
                throw std::invalid_argument(std::string(kind) + " '" + name
                        + "' declared twice");
            }

            if (entry.*slotMember == slot)
            {
                throw std::invalid_argument(std::string(kind) + " '" + name
                        + "' reuses " + slotName + " " + std::to_string(slot)
                        + " of '" + entry.name + "'");
            }
        }
    }

    std::vector<ShaderInterface::Member> layoutStd140(
            std::string const& blockName,
            std::vector<ShaderInterface::MemberDecl> const& decls,
            size_t& blockSize)
    {
        std::vector<ShaderInterface::Member> members;
        members.reserve(decls.size());
        size_t offset = 0;

        for (auto const& decl : decls)
        {
            if (decl.arraySize < 0)
            {
                throw std::invalid_argument("Member '" + blockName + "."
                        + decl.name + "' has a negative array size");
            }

            for (auto const& member : members)
            {
                if (member.name == decl.name)
                {
                    throw std::invalid_argument("Member '" + blockName + "."
                            + decl.name + "' declared twice");
                }
            }

            Std140Type const type = std140Type(decl.type);
            size_t alignment = type.alignment;
            size_t arrayStride = 0;
            size_t size = type.size;

            if (decl.arraySize > 0)
            {
                arrayStride = roundUp(type.size, vec4Size);
                alignment = vec4Size;
                size = arrayStride * static_cast<size_t>(decl.arraySize);
            }

            offset = roundUp(offset, alignment);
            members.push_back(ShaderInterface::Member{ decl.name, decl.type,
                    decl.arraySize, offset, arrayStride });
            offset += size;
        }

        blockSize = roundUp(offset, vec4Size);
        return members;
    }
} // anonymous namespace

std::string_view toString(UniformType type)
{
    switch (type)
    {
        case UniformType::Float: return "float";
        case UniformType::Float2: return "vec2";
        case UniformType::Float3: return "vec3";
        case UniformType::Float4: return "vec4";
        case UniformType::Int: return "int";
        case UniformType::Int2: return "ivec2";
        case UniformType::Int3: return "ivec3";
        case UniformType::Int4: return "ivec4";
        case UniformType::UInt: return "uint";
        case UniformType::UInt2: return "uvec2";
        case UniformType::UInt3: return "uvec3";
        case UniformType::UInt4: return "uvec4";
        case UniformType::Mat2: return "mat2";
        case UniformType::Mat3: return "mat3";
        case UniformType::Mat4: return "mat4";
    }

    return "unknown";
}

bool ShaderInterface::Attribute::operator==(Attribute const& rhs) const
{
    return name == rhs.name && location == rhs.location && type == rhs.type
        && size == rhs.size;
}

bool ShaderInterface::Attribute::operator!=(Attribute const& rhs) const
{
    return !(*this == rhs);
}

size_t ShaderInterface::Member::getSize() const
{
    if (arraySize > 0)
        return arrayStride * static_cast<size_t>(arraySize);

    return std140Type(type).size;
}

bool ShaderInterface::Member::operator==(Member const& rhs) const
{
    return name == rhs.name && type == rhs.type && arraySize == rhs.arraySize
        && offset == rhs.offset && arrayStride == rhs.arrayStride;
}

bool ShaderInterface::Member::operator!=(Member const& rhs) const
{
    return !(*this == rhs);
}

ShaderInterface::Member const* ShaderInterface::Block::findMember(
        std::string_view memberName) const
{
    for (auto const& member : members)
    {
        if (member.name == memberName)
            return &member;
    }

    return nullptr;
}

bool ShaderInterface::Block::operator==(Block const& rhs) const
{
    return name == rhs.name && binding == rhs.binding
        && layout == rhs.layout && members == rhs.members
        && size == rhs.size;
}

bool ShaderInterface::Block::operator!=(Block const& rhs) const
{
    return !(*this == rhs);
}

bool ShaderInterface::Sampler::operator==(Sampler const& rhs) const
{
    return name == rhs.name && unit == rhs.unit && type == rhs.type;
}

bool ShaderInterface::Sampler::operator!=(Sampler const& rhs) const
{
    return !(*this == rhs);
}

ShaderInterface::ShaderInterface()
{
}

ShaderInterface::~ShaderInterface()
{
}

ShaderInterface& ShaderInterface::attribute(std::string name, int location,
        Type type, int size)
{
    requireUnique(attributes_, name, location, "Attribute", "location",
            &Attribute::location);

    if (size < 1 || size > 4)
    {
        throw std::invalid_argument("Attribute '" + name + "' has "
                + std::to_string(size) + " components");
    }

    attributes_.push_back(Attribute{ std::move(name), location, type, size });
    return *this;
}

ShaderInterface& ShaderInterface::block(std::string name, int binding,
        BlockLayout layout, std::vector<MemberDecl> members)
{
    requireUnique(blocks_, name, binding, "Block", "binding",
            &Block::binding);

    size_t size = 0;
    std::vector<Member> laidOut = layoutStd140(name, members, size);

    blocks_.push_back(Block{ std::move(name), binding, layout,
            std::move(laidOut), size });
    return *this;
}

ShaderInterface& ShaderInterface::block(Block block)
{
    requireUnique(blocks_, block.name, block.binding, "Block", "binding",
            &Block::binding);

    for (size_t i = 0; i < block.members.size(); ++i)
    {
        for (size_t j = 0; j < i; ++j)
        {
            if (block.members[i].name == block.members[j].name)
            {
                throw std::invalid_argument("Member '" + block.name + "."
                        + block.members[i].name + "' declared twice");
            }
        }
    }

    blocks_.push_back(std::move(block));
    return *this;
}

ShaderInterface& ShaderInterface::sampler(std::string name, int unit,
        SamplerType type)
{
    requireUnique(samplers_, name, unit, "Sampler", "unit", &Sampler::unit);

    samplers_.push_back(Sampler{ std::move(name), unit, type });
    return *this;
}

std::vector<ShaderInterface::Attribute> const&
ShaderInterface::getAttributes() const
{
    return attributes_;
}

std::vector<ShaderInterface::Block> const& ShaderInterface::getBlocks() const
{
    return blocks_;
}

std::vector<ShaderInterface::Sampler> const&
ShaderInterface::getSamplers() const
{
    return samplers_;
}

ShaderInterface::Attribute const* ShaderInterface::findAttribute(
        std::string_view name) const
{
    for (auto const& attribute : attributes_)
    {
        if (attribute.name == name)
            return &attribute;
    }

    return nullptr;
}

ShaderInterface::Block const* ShaderInterface::findBlock(
        std::string_view name) const
{
    for (auto const& block : blocks_)
    {
        if (block.name == name)
            return &block;
    }

    return nullptr;
}

ShaderInterface::Sampler const* ShaderInterface::findSampler(
        std::string_view name) const
{
    for (auto const& sampler : samplers_)
    {
        if (sampler.name == name)
            return &sampler;
    }

    return nullptr;
}

namespace
{
    std::string attributeString(ShaderInterface::Attribute const& a)
    {
        char const* types[] = { "float", "int", "uint" };
        return std::string(types[a.type]) + " x" + std::to_string(a.size)
            + " at location " + std::to_string(a.location);
    }

    std::string memberString(ShaderInterface::Member const& m)
    {
        std::string result = std::string(toString(m.type));
        if (m.arraySize > 0)
        {
            result += "[" + std::to_string(m.arraySize) + "] (stride "
                + std::to_string(m.arrayStride) + ")";
        }

        return result + " at offset " + std::to_string(m.offset);
    }

    void compareBlock(ShaderInterface::Block const& declared,
            ShaderInterface::Block const& compiled,
            std::vector<std::string>& out)
    {
        if (declared.binding != compiled.binding)
        {
            out.push_back("block '" + declared.name + "': declared binding "
                    + std::to_string(declared.binding) + ", compiled "
                    + std::to_string(compiled.binding));
        }

        if (compiled.size < declared.size)
        {
            out.push_back("block '" + declared.name + "': declared size "
                    + std::to_string(declared.size) + ", compiled "
                    + std::to_string(compiled.size));
        }

        for (auto const& member : declared.members)
        {
            auto const* other = compiled.findMember(member.name);
            if (!other)
            {
                out.push_back("member '" + declared.name + "." + member.name
                        + "' is declared but not compiled");
            }
            else if (*other != member)
            {
                out.push_back("member '" + declared.name + "." + member.name
                        + "': declared " + memberString(member)
                        + ", compiled " + memberString(*other));
            }
        }

        for (auto const& member : compiled.members)
        {
            if (!declared.findMember(member.name))
            {
                out.push_back("member '" + declared.name + "." + member.name
                        + "' is compiled but not declared");
            }
        }
    }
} // anonymous namespace

std::vector<std::string> findMismatches(ShaderInterface const& declared,
        ShaderInterface const& compiled)
{
    std::vector<std::string> out;

    for (auto const& attribute : declared.getAttributes())
    {
        auto const* other = compiled.findAttribute(attribute.name);
        if (!other)
        {
            out.push_back("attribute '" + attribute.name
                    + "' is declared but not compiled");
        }
        else if (*other != attribute)
        {
            out.push_back("attribute '" + attribute.name + "': declared "
                    + attributeString(attribute) + ", compiled "
                    + attributeString(*other));
        }
    }

    for (auto const& attribute : compiled.getAttributes())
    {
        if (!declared.findAttribute(attribute.name))
        {
            out.push_back("attribute '" + attribute.name
                    + "' is compiled but not declared");
        }
    }

    for (auto const& block : declared.getBlocks())
    {
        auto const* other = compiled.findBlock(block.name);
        if (!other)
        {
            out.push_back("block '" + block.name
                    + "' is declared but not compiled");
        }
        else
            compareBlock(block, *other, out);
    }

    for (auto const& block : compiled.getBlocks())
    {
        if (!declared.findBlock(block.name))
        {
            out.push_back("block '" + block.name
                    + "' is compiled but not declared");
        }
    }

    for (auto const& sampler : declared.getSamplers())
    {
        auto const* other = compiled.findSampler(sampler.name);
        if (!other)
        {
            out.push_back("sampler '" + sampler.name
                    + "' is declared but not compiled");
        }
        else if (*other != sampler)
        {
            out.push_back("sampler '" + sampler.name + "': declared unit "
                    + std::to_string(sampler.unit) + ", compiled "
                    + std::to_string(other->unit));
        }
    }

    for (auto const& sampler : compiled.getSamplers())
    {
        if (!declared.findSampler(sampler.name))
        {
            out.push_back("sampler '" + sampler.name
                    + "' is compiled but not declared");
        }
    }

    return out;
}

bool ShaderInterface::operator==(ShaderInterface const& rhs) const
{
    return attributes_ == rhs.attributes_ && blocks_ == rhs.blocks_
        && samplers_ == rhs.samplers_;
}

bool ShaderInterface::operator!=(ShaderInterface const& rhs) const
{
    return !(*this == rhs);
}

} // namespace ase
